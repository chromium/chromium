// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/nacl_file_host.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/nacl/browser/bad_message.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_browser_delegate.h"
#include "components/nacl/browser/nacl_host_message_filter.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "ipc/ipc_platform_file.h"

using content::BrowserThread;

namespace {

// Force a prefix to prevent user from opening "magic" files.
const char* kExpectedFilePrefix = "pnacl_public_";

// Restrict PNaCl file lengths to reduce likelyhood of hitting bugs
// in file name limit error-handling-code-paths, etc.
const size_t kMaxFileLength = 40;

void NotifyRendererOfError(
    nacl::NaClHostMessageFilter* nacl_host_message_filter,
    IPC::Message* reply_msg) {
  reply_msg->set_reply_error();
  nacl_host_message_filter->Send(reply_msg);
}

typedef void (*WriteFileInfoReply)(IPC::Message* reply_msg,
                                   const IPC::PlatformFileForTransit& file_desc,
                                   const uint64_t& file_token_lo,
                                   const uint64_t& file_token_hi);

void DoRegisterOpenedNaClExecutableFile(
    scoped_refptr<nacl::NaClHostMessageFilter> nacl_host_message_filter,
    base::File file,
    base::FilePath file_path,
    IPC::Message* reply_msg,
    WriteFileInfoReply write_reply_message) {
  // IO thread owns the NaClBrowser singleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  nacl::NaClBrowser* nacl_browser = nacl::NaClBrowser::GetInstance();
  uint64_t file_token_lo = 0;
  uint64_t file_token_hi = 0;
  nacl_browser->PutFilePath(file_path, &file_token_lo, &file_token_hi);

  IPC::PlatformFileForTransit file_desc =
      IPC::TakePlatformFileForTransit(std::move(file));

  write_reply_message(reply_msg, file_desc, file_token_lo, file_token_hi);
  nacl_host_message_filter->Send(reply_msg);
}

void DoOpenPnaclFile(
    scoped_refptr<nacl::NaClHostMessageFilter> nacl_host_message_filter,
    const std::string& filename,
    bool is_executable,
    IPC::Message* reply_msg) {
  base::FilePath full_filepath;

  // PNaCl must be installed.
  base::FilePath pnacl_dir;
  if (!nacl::NaClBrowser::GetDelegate()->GetPnaclDirectory(&pnacl_dir) ||
      !base::PathExists(pnacl_dir)) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }

  // Do some validation.
  if (!nacl_file_host::PnaclCanOpenFile(filename, &full_filepath)) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }

  base::File file_to_open = nacl::OpenNaClReadExecImpl(full_filepath,
                                                       is_executable);
  if (!file_to_open.IsValid()) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }

  // This function is running on the blocking pool, but the path needs to be
  // registered in a structure owned by the IO thread.
  // Not all PNaCl files are executable. Only register those that are
  // executable in the NaCl file_path cache.
  if (is_executable) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&DoRegisterOpenedNaClExecutableFile,
                       nacl_host_message_filter, std::move(file_to_open),
                       full_filepath, reply_msg,
                       static_cast<WriteFileInfoReply>(
                           NaClHostMsg_GetReadonlyPnaclFD::WriteReplyParams)));
  } else {
    IPC::PlatformFileForTransit target_desc =
        IPC::TakePlatformFileForTransit(std::move(file_to_open));
    uint64_t dummy_file_token = 0;
    NaClHostMsg_GetReadonlyPnaclFD::WriteReplyParams(
        reply_msg, target_desc, dummy_file_token, dummy_file_token);
    nacl_host_message_filter->Send(reply_msg);
  }
}

// Convert the file URL into a file descriptor.
// This function is security sensitive.  Be sure to check with a security
// person before you modify it.
void DoOpenNaClExecutableOnThreadPool(
    scoped_refptr<nacl::NaClHostMessageFilter> nacl_host_message_filter,
    const GURL& file_url,
    bool enable_validation_caching,
    NaClBrowserDelegate::MapUrlToLocalFilePathCallback map_url_callback,
    IPC::Message* reply_msg) {
  base::FilePath file_path;
  if (!map_url_callback.Run(file_url, true /* use_blocking_api */,
                            &file_path)) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }

  base::File file = nacl::OpenNaClReadExecImpl(file_path,
                                               true /* is_executable */);
  if (file.IsValid()) {
    // Opening a NaCl executable works with or without validation caching.
    // Validation caching requires that the file descriptor is registered now
    // for later use, which will save time.
    // When validation caching isn't used (e.g. Non-SFI mode), there is no
    // reason to do that unnecessary registration.
    if (enable_validation_caching) {
      // This function is running on the blocking pool, but the path needs to be
      // registered in a structure owned by the IO thread.
      base::PostTask(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(
              &DoRegisterOpenedNaClExecutableFile, nacl_host_message_filter,
              std::move(file), file_path, reply_msg,
              static_cast<WriteFileInfoReply>(
                  NaClHostMsg_OpenNaClExecutable::WriteReplyParams)));
    } else {
      IPC::PlatformFileForTransit file_desc =
          IPC::TakePlatformFileForTransit(std::move(file));
      uint64_t dummy_file_token = 0;
      NaClHostMsg_OpenNaClExecutable::WriteReplyParams(
          reply_msg, file_desc, dummy_file_token, dummy_file_token);
      nacl_host_message_filter->Send(reply_msg);
    }
  } else {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }
}

}  // namespace

namespace nacl_file_host {

void GetReadonlyPnaclFd(
    scoped_refptr<nacl::NaClHostMessageFilter> nacl_host_message_filter,
    const std::string& filename,
    bool is_executable,
    IPC::Message* reply_msg) {
  base::PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
                 base::BindOnce(&DoOpenPnaclFile, nacl_host_message_filter,
                                filename, is_executable, reply_msg));
}

// This function is security sensitive.  Be sure to check with a security
// person before you modify it.
bool PnaclCanOpenFile(const std::string& filename,
                      base::FilePath* file_to_open) {
  if (filename.length() > kMaxFileLength)
    return false;

  if (filename.empty())
    return false;

  // Restrict character set of the file name to something really simple
  // (a-z, 0-9, and underscores).
  for (size_t i = 0; i < filename.length(); ++i) {
    char charAt = filename[i];
    if (charAt < 'a' || charAt > 'z')
      if (charAt < '0' || charAt > '9')
        if (charAt != '_')
          return false;
  }

  // PNaCl must be installed.
  base::FilePath pnacl_dir;
  if (!nacl::NaClBrowser::GetDelegate()->GetPnaclDirectory(&pnacl_dir) ||
      pnacl_dir.empty())
    return false;

  // Prepend the prefix to restrict files to a whitelisted set.
  base::FilePath full_path = pnacl_dir.AppendASCII(
      std::string(kExpectedFilePrefix) + filename);
  *file_to_open = full_path;
  return true;
}

void OpenNaClExecutable(
    scoped_refptr<nacl::NaClHostMessageFilter> nacl_host_message_filter,
    int render_view_id,
    const GURL& file_url,
    bool enable_validation_caching,
    IPC::Message* reply_msg) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&OpenNaClExecutable, nacl_host_message_filter,
                                  render_view_id, file_url,
                                  enable_validation_caching, reply_msg));
    return;
  }

  // Make sure render_view_id is valid and that the URL is a part of the
  // render view's site. Without these checks, apps could probe the extension
  // directory or run NaCl code from other extensions.
  content::RenderViewHost* rvh = content::RenderViewHost::FromID(
      nacl_host_message_filter->render_process_id(), render_view_id);
  if (!rvh) {
    nacl::bad_message::ReceivedBadMessage(
        nacl_host_message_filter.get(),
        nacl::bad_message::NFH_OPEN_EXECUTABLE_BAD_ROUTING_ID);
    delete reply_msg;
    return;
  }
  content::SiteInstance* site_instance = rvh->GetSiteInstance();
  if (!site_instance->IsSameSiteWithURL(file_url)) {
    NotifyRendererOfError(nacl_host_message_filter.get(), reply_msg);
    return;
  }

  auto map_url_callback =
      nacl::NaClBrowser::GetDelegate()->GetMapUrlToLocalFilePathCallback(
          nacl_host_message_filter->profile_directory());

  // The URL is part of the current app. Now query the extension system for the
  // file path and convert that to a file descriptor. This should be done on a
  // blocking pool thread.
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&DoOpenNaClExecutableOnThreadPool,
                     nacl_host_message_filter, file_url,
                     enable_validation_caching, map_url_callback, reply_msg));
}

}  // namespace nacl_file_host

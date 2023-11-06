// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/nacl_host_message_filter.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/nacl/browser/bad_message.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_file_host.h"
#include "components/nacl/browser/nacl_process_host.h"
#include "components/nacl/browser/pnacl_host.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "url/gurl.h"

namespace nacl {

namespace {

// The maximum number of resource file handles the browser process accepts. Use
// 200 because ARC's nmf has ~128 resource files as of May 2015. This prevents
// untrusted code filling the FD/handle table.
const size_t kMaxPreOpenResourceFiles = 200;

ppapi::PpapiPermissions GetNaClPermissions(
    uint32_t permission_bits,
    content::BrowserContext* browser_context,
    const GURL& document_url) {
  // Default permissions keep NaCl plugins backwards-compatible, but don't
  // grant any other special permissions. We don't want a compromised renderer
  // to be able to start a NaCl plugin with Dev or Flash permissions which may
  // expand the surface area of the sandbox.
  uint32_t nacl_permissions = ppapi::PERMISSION_DEFAULT;
  if (content::PluginService::GetInstance()->PpapiDevChannelSupported(
          browser_context, document_url))
    nacl_permissions |= ppapi::PERMISSION_DEV_CHANNEL;
  return ppapi::PpapiPermissions::GetForCommandLine(nacl_permissions);
}

ppapi::PpapiPermissions GetPpapiPermissions(
    uint32_t permission_bits,
    content::RenderFrameHost* frame_host) {
  // We get the URL from WebContents from the RenderViewHost, since we don't
  // have a BrowserPpapiHost yet.
  content::RenderViewHost* view_host = frame_host->GetRenderViewHost();
  if (!view_host)
    return ppapi::PpapiPermissions();
  GURL document_url;
  content::WebContents* contents =
      content::WebContents::FromRenderViewHost(view_host);
  if (contents)
    document_url = contents->GetLastCommittedURL();
  return GetNaClPermissions(permission_bits, frame_host->GetBrowserContext(),
                            document_url);
}

void CallPnaclHostRendererClosing(int render_process_id) {
  pnacl::PnaclHost::GetInstance()->RendererClosing(render_process_id);
}

}  // namespace

NaClHostMessageFilter::NaClHostMessageFilter(
    int render_process_id,
    bool is_off_the_record,
    const base::FilePath& profile_directory)
    : BrowserMessageFilter(NaClHostMsgStart),
      render_process_id_(render_process_id),
      off_the_record_(is_off_the_record),
      profile_directory_(profile_directory) {}

NaClHostMessageFilter::~NaClHostMessageFilter() {
}

void NaClHostMessageFilter::OnChannelClosing() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(CallPnaclHostRendererClosing, render_process_id_));
}

void NaClHostMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
#if BUILDFLAG(ENABLE_NACL)
  if (message.type() == NaClHostMsg_LaunchNaCl::ID) {
    *thread = content::BrowserThread::UI;
  } else if (message.type() == NaClHostMsg_GetReadonlyPnaclFD::ID ||
             message.type() == NaClHostMsg_NaClCreateTemporaryFile::ID ||
             message.type() == NaClHostMsg_NexeTempFileRequest::ID ||
             message.type() == NaClHostMsg_ReportTranslationFinished::ID) {
    *thread = content::BrowserThread::UI;
  }
#endif
}

bool NaClHostMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClHostMessageFilter, message)
#if BUILDFLAG(ENABLE_NACL)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_LaunchNaCl, OnLaunchNaCl)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_GetReadonlyPnaclFD,
                                    OnGetReadonlyPnaclFd)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_NaClCreateTemporaryFile,
                                    OnNaClCreateTemporaryFile)
    IPC_MESSAGE_HANDLER(NaClHostMsg_NexeTempFileRequest,
                        OnGetNexeFd)
    IPC_MESSAGE_HANDLER(NaClHostMsg_ReportTranslationFinished,
                        OnTranslationFinished)
    IPC_MESSAGE_HANDLER(NaClHostMsg_MissingArchError,
                        OnMissingArchError)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_OpenNaClExecutable,
                                    OnOpenNaClExecutable)
    IPC_MESSAGE_HANDLER(NaClHostMsg_NaClGetNumProcessors,
                        OnNaClGetNumProcessors)
    IPC_MESSAGE_HANDLER(NaClHostMsg_NaClDebugEnabledForURL,
                        OnNaClDebugEnabledForURL)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NaClHostMessageFilter::OnLaunchNaCl(
    const nacl::NaClLaunchParams& launch_params,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto map_url_callback =
      nacl::NaClBrowser::GetDelegate()->GetMapUrlToLocalFilePathCallback(
          profile_directory_);

  // If we're running llc or ld for the PNaCl translator, we don't need to look
  // up permissions, and we don't have the right browser state to look up some
  // of the allowed parameters anyway.
  if (launch_params.process_type == kPNaClTranslatorProcessType) {
    uint32_t perms = launch_params.permission_bits & ppapi::PERMISSION_DEV;
    LaunchNaClContinuationOnUIThread(
        launch_params, reply_msg, std::vector<NaClResourcePrefetchResult>(),
        ppapi::PpapiPermissions(perms), map_url_callback);
    return;
  }
  LaunchNaClContinuation(launch_params, reply_msg, map_url_callback);
}

void NaClHostMessageFilter::LaunchNaClContinuation(
    const nacl::NaClLaunchParams& launch_params,
    IPC::Message* reply_msg,
    NaClBrowserDelegate::MapUrlToLocalFilePathCallback map_url_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      render_process_id(), launch_params.render_frame_id);
  if (!rfh) {
    bad_message::ReceivedBadMessage(
        this, bad_message::NHMF_LAUNCH_CONTINUATION_BAD_ROUTING_ID);
    delete reply_msg;
    return;
  }

  ppapi::PpapiPermissions permissions =
      GetPpapiPermissions(launch_params.permission_bits, rfh);

  nacl::NaClLaunchParams safe_launch_params(launch_params);
  safe_launch_params.resource_prefetch_request_list.clear();

  const std::vector<NaClResourcePrefetchRequest>& original_request_list =
      launch_params.resource_prefetch_request_list;
  content::SiteInstance* site_instance = rfh->GetSiteInstance();
  for (const auto& original_request : original_request_list) {
    GURL gurl(original_request.resource_url);
    // Important security check: Do the same check as OpenNaClExecutable()
    // in nacl_file_host.cc.
    if (!site_instance->IsSameSiteWithURL(gurl))
      continue;
    safe_launch_params.resource_prefetch_request_list.push_back(
        original_request);
  }

  // Process a list of resource file URLs in
  // |launch_params.resource_files_to_prefetch|.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&NaClHostMessageFilter::BatchOpenResourceFiles, this,
                     safe_launch_params, reply_msg, permissions,
                     map_url_callback));
}

void NaClHostMessageFilter::BatchOpenResourceFiles(
    const nacl::NaClLaunchParams& launch_params,
    IPC::Message* reply_msg,
    ppapi::PpapiPermissions permissions,
    NaClBrowserDelegate::MapUrlToLocalFilePathCallback map_url_callback) {
  std::vector<NaClResourcePrefetchResult> prefetched_resource_files;
  const std::vector<NaClResourcePrefetchRequest>& request_list =
      launch_params.resource_prefetch_request_list;
  for (size_t i = 0; i < request_list.size(); ++i) {
    GURL gurl(request_list[i].resource_url);
    base::FilePath file_path_metadata;
    if (!map_url_callback.Run(gurl,
                              true,  // use_blocking_api
                              &file_path_metadata)) {
      continue;
    }
    base::File file = nacl::OpenNaClReadExecImpl(
        file_path_metadata, true /* is_executable */);
    if (!file.IsValid())
      continue;

    prefetched_resource_files.push_back(NaClResourcePrefetchResult(
        IPC::TakePlatformFileForTransit(std::move(file)), file_path_metadata,
        request_list[i].file_key));

    if (prefetched_resource_files.size() >= kMaxPreOpenResourceFiles)
      break;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NaClHostMessageFilter::LaunchNaClContinuationOnUIThread,
                     this, launch_params, reply_msg, prefetched_resource_files,
                     permissions, map_url_callback));
}

void NaClHostMessageFilter::LaunchNaClContinuationOnUIThread(
    const nacl::NaClLaunchParams& launch_params,
    IPC::Message* reply_msg,
    const std::vector<NaClResourcePrefetchResult>& prefetched_resource_files,
    ppapi::PpapiPermissions permissions,
    NaClBrowserDelegate::MapUrlToLocalFilePathCallback map_url_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NaClFileToken nexe_token = {
      launch_params.nexe_token_lo,  // lo
      launch_params.nexe_token_hi   // hi
  };

  base::PlatformFile nexe_file =
      IPC::PlatformFileForTransitToPlatformFile(launch_params.nexe_file);

  NaClProcessHost* host = new NaClProcessHost(
      GURL(launch_params.manifest_url), base::File(nexe_file), nexe_token,
      prefetched_resource_files, permissions, launch_params.permission_bits,
      off_the_record_, launch_params.process_type, profile_directory_);
  GURL manifest_url(launch_params.manifest_url);
  base::FilePath manifest_path;
  // We're calling MapUrlToLocalFilePath with the non-blocking API
  // because we're running in the I/O thread. Ideally we'd use the other path,
  // which would cover more cases.
  map_url_callback.Run(manifest_url, false /* use_blocking_api */,
                       &manifest_path);
  host->Launch(this, reply_msg, manifest_path);
}

void NaClHostMessageFilter::OnGetReadonlyPnaclFd(
    const std::string& filename, bool is_executable, IPC::Message* reply_msg) {
  // This posts a task to another thread, but the renderer will
  // block until the reply is sent.
  nacl_file_host::GetReadonlyPnaclFd(this, filename, is_executable, reply_msg);

  // This is the first message we receive from the renderer once it knows we
  // want to use PNaCl, so start the translation cache initialization here.
  pnacl::PnaclHost::GetInstance()->Init();
}

// Return the temporary file via a reply to the
// NaClHostMsg_NaClCreateTemporaryFile sync message.
void NaClHostMessageFilter::SyncReturnTemporaryFile(
    IPC::Message* reply_msg,
    base::File file) {
  if (file.IsValid()) {
    NaClHostMsg_NaClCreateTemporaryFile::WriteReplyParams(
        reply_msg, IPC::TakePlatformFileForTransit(std::move(file)));
  } else {
    reply_msg->set_reply_error();
  }
  Send(reply_msg);
}

void NaClHostMessageFilter::OnNaClCreateTemporaryFile(
    IPC::Message* reply_msg) {
  pnacl::PnaclHost::GetInstance()->CreateTemporaryFile(base::BindRepeating(
      &NaClHostMessageFilter::SyncReturnTemporaryFile, this, reply_msg));
}

void NaClHostMessageFilter::AsyncReturnTemporaryFile(
    int pp_instance,
    const base::File& file,
    bool is_hit) {
  IPC::PlatformFileForTransit fd = IPC::InvalidPlatformFileForTransit();
  if (file.IsValid()) {
    // Don't close our copy of the handle, because PnaclHost will use it
    // when the translation finishes.
    fd = IPC::GetPlatformFileForTransit(file.GetPlatformFile(), false);
  }
  Send(new NaClViewMsg_NexeTempFileReply(pp_instance, is_hit, fd));
}

void NaClHostMessageFilter::OnNaClGetNumProcessors(int* num_processors) {
  *num_processors = base::SysInfo::NumberOfProcessors();
}

void NaClHostMessageFilter::OnGetNexeFd(
    int pp_instance,
    const nacl::PnaclCacheInfo& cache_info) {
  if (!cache_info.pexe_url.is_valid()) {
    LOG(ERROR) << "Bad URL received from GetNexeFd: " <<
        cache_info.pexe_url.possibly_invalid_spec();
    bad_message::ReceivedBadMessage(this,
                                    bad_message::NHMF_GET_NEXE_FD_BAD_URL);
    return;
  }

  pnacl::PnaclHost::GetInstance()->GetNexeFd(
      render_process_id_, pp_instance, off_the_record_, cache_info,
      base::BindRepeating(&NaClHostMessageFilter::AsyncReturnTemporaryFile,
                          this, pp_instance));
}

void NaClHostMessageFilter::OnTranslationFinished(int instance, bool success) {
  pnacl::PnaclHost::GetInstance()->TranslationFinished(
      render_process_id_, instance, success);
}

void NaClHostMessageFilter::OnMissingArchError(int render_frame_id) {
  nacl::NaClBrowser::GetDelegate()->ShowMissingArchInfobar(render_process_id_,
                                                           render_frame_id);
}

void NaClHostMessageFilter::OnOpenNaClExecutable(int render_frame_id,
                                                 const GURL& file_url,
                                                 IPC::Message* reply_msg) {
  nacl_file_host::OpenNaClExecutable(this, render_frame_id, file_url,
                                     reply_msg);
}

void NaClHostMessageFilter::OnNaClDebugEnabledForURL(const GURL& nmf_url,
                                                     bool* should_debug) {
  *should_debug =
      nacl::NaClBrowser::GetDelegate()->URLMatchesDebugPatterns(nmf_url);
}

}  // namespace nacl

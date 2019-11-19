// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/nacl_host_message_filter.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
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

ppapi::PpapiPermissions GetPpapiPermissions(uint32_t permission_bits,
                                            int render_process_id,
                                            int render_view_id) {
  // We get the URL from WebContents from the RenderViewHost, since we don't
  // have a BrowserPpapiHost yet.
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  content::RenderViewHost* view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (!view_host)
    return ppapi::PpapiPermissions();
  GURL document_url;
  content::WebContents* contents =
      content::WebContents::FromRenderViewHost(view_host);
  if (contents)
    document_url = contents->GetLastCommittedURL();
  return GetNaClPermissions(permission_bits,
                            host->GetBrowserContext(),
                            document_url);
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
  pnacl::PnaclHost::GetInstance()->RendererClosing(render_process_id_);
}

void NaClHostMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
#if BUILDFLAG(ENABLE_NACL)
  if (message.type() == NaClHostMsg_LaunchNaCl::ID)
    *thread = content::BrowserThread::UI;
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

  bool nonsfi_mode_allowed = false;
#if defined(OS_CHROMEOS) && \
    (defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARMEL))
  nonsfi_mode_allowed = NaClBrowser::GetDelegate()->IsNonSfiModeAllowed(
      profile_directory_, GURL(launch_params.manifest_url));
#endif

  auto map_url_callback =
      nacl::NaClBrowser::GetDelegate()->GetMapUrlToLocalFilePathCallback(
          profile_directory_);

  // If we're running llc or ld for the PNaCl translator, we don't need to look
  // up permissions, and we don't have the right browser state to look up some
  // of the whitelisting parameters anyway.
  if (launch_params.process_type == kPNaClTranslatorProcessType) {
    uint32_t perms = launch_params.permission_bits & ppapi::PERMISSION_DEV;
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&NaClHostMessageFilter::LaunchNaClContinuationOnIOThread,
                       this, launch_params, reply_msg,
                       std::vector<NaClResourcePrefetchResult>(),
                       ppapi::PpapiPermissions(perms), nonsfi_mode_allowed,
                       map_url_callback));
    return;
  }
  LaunchNaClContinuation(launch_params, reply_msg, nonsfi_mode_allowed,
                         map_url_callback);
}

void NaClHostMessageFilter::LaunchNaClContinuation(
    const nacl::NaClLaunchParams& launch_params,
    IPC::Message* reply_msg,
    bool nonsfi_mode_allowed,
    NaClBrowserDelegate::MapUrlToLocalFilePathCallback map_url_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ppapi::PpapiPermissions permissions =
      GetPpapiPermissions(launch_params.permission_bits,
                          render_process_id_,
                          launch_params.render_view_id);

  content::RenderViewHost* rvh = content::RenderViewHost::FromID(
      render_process_id(), launch_params.render_view_id);
  if (!rvh) {
    bad_message::ReceivedBadMessage(
        this, bad_message::NHMF_LAUNCH_CONTINUATION_BAD_ROUTING_ID);
    delete reply_msg;
    return;
  }

  nacl::NaClLaunchParams safe_launch_params(launch_params);
  safe_launch_params.resource_prefetch_request_list.clear();

  // TODO(yusukes): Fix NaClProcessHost::~NaClProcessHost() and remove the
  // ifdef.
#if !defined(OS_WIN)
  const std::vector<NaClResourcePrefetchRequest>& original_request_list =
      launch_params.resource_prefetch_request_list;
  content::SiteInstance* site_instance = rvh->GetSiteInstance();
  for (size_t i = 0; i < original_request_list.size(); ++i) {
    GURL gurl(original_request_list[i].resource_url);
    // Important security check: Do the same check as OpenNaClExecutable()
    // in nacl_file_host.cc.
    if (!site_instance->IsSameSiteWithURL(gurl))
      continue;
    safe_launch_params.resource_prefetch_request_list.push_back(
        original_request_list[i]);
  }
#endif

  // Process a list of resource file URLs in
  // |launch_params.resource_files_to_prefetch|.
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&NaClHostMessageFilter::BatchOpenResourceFiles, this,
                     safe_launch_params, reply_msg, permissions,
                     nonsfi_mode_allowed, map_url_callback));
}

void NaClHostMessageFilter::BatchOpenResourceFiles(
    const nacl::NaClLaunchParams& launch_params,
    IPC::Message* reply_msg,
    ppapi::PpapiPermissions permissions,
    bool nonsfi_mode_allowed,
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

  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&NaClHostMessageFilter::LaunchNaClContinuationOnIOThread,
                     this, launch_params, reply_msg, prefetched_resource_files,
                     permissions, nonsfi_mode_allowed, map_url_callback));
}

void NaClHostMessageFilter::LaunchNaClContinuationOnIOThread(
    const nacl::NaClLaunchParams& launch_params,
    IPC::Message* reply_msg,
    const std::vector<NaClResourcePrefetchResult>& prefetched_resource_files,
    ppapi::PpapiPermissions permissions,
    bool nonsfi_mode_allowed,
    NaClBrowserDelegate::MapUrlToLocalFilePathCallback map_url_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  NaClFileToken nexe_token = {
      launch_params.nexe_token_lo,  // lo
      launch_params.nexe_token_hi   // hi
  };

  base::PlatformFile nexe_file =
      IPC::PlatformFileForTransitToPlatformFile(launch_params.nexe_file);

  NaClProcessHost* host = new NaClProcessHost(
      GURL(launch_params.manifest_url), base::File(nexe_file), nexe_token,
      prefetched_resource_files, permissions, launch_params.render_view_id,
      launch_params.permission_bits, launch_params.uses_nonsfi_mode,
      nonsfi_mode_allowed, off_the_record_, launch_params.process_type,
      profile_directory_);
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
  pnacl::PnaclHost::GetInstance()->CreateTemporaryFile(
      base::Bind(&NaClHostMessageFilter::SyncReturnTemporaryFile,
                 this,
                 reply_msg));
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
    int render_view_id,
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
      render_process_id_,
      render_view_id,
      pp_instance,
      off_the_record_,
      cache_info,
      base::Bind(&NaClHostMessageFilter::AsyncReturnTemporaryFile,
                 this,
                 pp_instance));
}

void NaClHostMessageFilter::OnTranslationFinished(int instance, bool success) {
  pnacl::PnaclHost::GetInstance()->TranslationFinished(
      render_process_id_, instance, success);
}

void NaClHostMessageFilter::OnMissingArchError(int render_view_id) {
  nacl::NaClBrowser::GetDelegate()->
      ShowMissingArchInfobar(render_process_id_, render_view_id);
}

void NaClHostMessageFilter::OnOpenNaClExecutable(
    int render_view_id,
    const GURL& file_url,
    bool enable_validation_caching,
    IPC::Message* reply_msg) {
  nacl_file_host::OpenNaClExecutable(this,
                                     render_view_id,
                                     file_url,
                                     enable_validation_caching,
                                     reply_msg);
}

void NaClHostMessageFilter::OnNaClDebugEnabledForURL(const GURL& nmf_url,
                                                     bool* should_debug) {
  *should_debug =
      nacl::NaClBrowser::GetDelegate()->URLMatchesDebugPatterns(nmf_url);
}

}  // namespace nacl

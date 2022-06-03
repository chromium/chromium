// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/nexe_load_manager.h"

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/renderer/histogram.h"
#include "components/nacl/renderer/manifest_service_channel.h"
#include "components/nacl/renderer/platform_info.h"
#include "components/nacl/renderer/pnacl_translation_resource_host.h"
#include "components/nacl/renderer/progress_event.h"
#include "components/nacl/renderer/trusted_plugin_channel.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_view.h"
#include "v8/include/v8.h"

namespace nacl {

namespace {

const char* const kTypeAttribute = "type";
// The "src" attribute of the <embed> tag.  The value is expected to be either
// a URL or URI pointing to the manifest file (which is expected to contain
// JSON matching ISAs with .nexe URLs).
const char* const kSrcManifestAttribute = "src";
// The "nacl" attribute of the <embed> tag.  We use the value of this attribute
// to find the manifest file when NaCl is registered as a plugin for another
// MIME type because the "src" attribute is used to supply us with the resource
// of that MIME type that we're supposed to display.
const char* const kNaClManifestAttribute = "nacl";

const char* const kNaClMIMEType = "application/x-nacl";
const char* const kPNaClMIMEType = "application/x-pnacl";

static int GetRoutingID(PP_Instance instance) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());
  content::RendererPpapiHost *host =
      content::RendererPpapiHost::GetForPPInstance(instance);
  if (!host)
    return 0;
  return host->GetRoutingIDForWidget(instance);
}

std::string LookupAttribute(const std::map<std::string, std::string>& args,
                            const std::string& key) {
  auto it = args.find(key);
  if (it != args.end())
    return it->second;
  return std::string();
}

}  // namespace

NexeLoadManager::NexeLoadManager(PP_Instance pp_instance)
    : pp_instance_(pp_instance),
      nacl_ready_state_(PP_NACL_READY_STATE_UNSENT),
      nexe_error_reported_(false),
      is_installed_(false),
      exit_status_(-1),
      nexe_size_(0),
      plugin_instance_(content::PepperPluginInstance::Get(pp_instance)),
      nonsfi_(false) {
  set_exit_status(-1);
  SetLastError("");
  HistogramEnumerateOsArch(GetSandboxArch());
  if (plugin_instance_) {
    plugin_base_url_ = plugin_instance_->GetContainer()->GetDocument().Url();
  }
}

NexeLoadManager::~NexeLoadManager() {
  if (!nexe_error_reported_) {
    base::TimeDelta uptime = base::Time::Now() - ready_time_;
    HistogramTimeLarge("NaCl.ModuleUptime.Normal", uptime.InMilliseconds());
  }
}

void NexeLoadManager::NexeFileDidOpen(int32_t pp_error,
                                      const base::File& file,
                                      int32_t http_status,
                                      int64_t nexe_bytes_read,
                                      const std::string& url,
                                      base::TimeDelta time_since_open) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());
  VLOG(1) << "Plugin::NexeFileDidOpen (pp_error=" << pp_error << ")";
  HistogramHTTPStatusCode(
      is_installed_ ? "NaCl.HttpStatusCodeClass.Nexe.InstalledApp" :
                      "NaCl.HttpStatusCodeClass.Nexe.NotInstalledApp",
      http_status);

  if (pp_error != PP_OK || !file.IsValid()) {
    if (pp_error == PP_ERROR_ABORTED) {
      ReportLoadAbort();
    } else if (pp_error == PP_ERROR_NOACCESS) {
      ReportLoadError(PP_NACL_ERROR_NEXE_NOACCESS_URL,
                      "access to nexe url was denied.");
    } else {
      ReportLoadError(PP_NACL_ERROR_NEXE_LOAD_URL,
                      "could not load nexe url.");
    }
  } else if (nexe_bytes_read == -1) {
    ReportLoadError(PP_NACL_ERROR_NEXE_STAT, "could not stat nexe file.");
  } else {
    // TODO(dmichael): Can we avoid stashing away so much state?
    nexe_size_ = nexe_bytes_read;
    HistogramSizeKB("NaCl.Perf.Size.Nexe",
                    static_cast<int32_t>(nexe_size_ / 1024));
    HistogramStartupTimeMedium(
        "NaCl.Perf.StartupTime.NexeDownload", time_since_open, nexe_size_);

    // Inform JavaScript that we successfully downloaded the nacl module.
    ProgressEvent progress_event(PP_NACL_EVENT_PROGRESS, url, true, nexe_size_,
                                 nexe_size_);
    DispatchProgressEvent(pp_instance_, progress_event);
    load_start_ = base::Time::Now();
  }
}

void NexeLoadManager::ReportLoadSuccess(const std::string& url,
                                        uint64_t loaded_bytes,
                                        uint64_t total_bytes) {
  ready_time_ = base::Time::Now();
  if (!IsPNaCl()) {
    base::TimeDelta load_module_time = ready_time_ - load_start_;
    HistogramStartupTimeSmall(
        "NaCl.Perf.StartupTime.LoadModule", load_module_time, nexe_size_);
    HistogramStartupTimeMedium(
        "NaCl.Perf.StartupTime.Total", ready_time_ - init_time_, nexe_size_);
  }

  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());
  set_nacl_ready_state(PP_NACL_READY_STATE_DONE);

  // Inform JavaScript that loading was successful and is complete.
  ProgressEvent load_event(PP_NACL_EVENT_LOAD, url, true, loaded_bytes,
                           total_bytes);
  DispatchProgressEvent(pp_instance_, load_event);

  ProgressEvent loadend_event(PP_NACL_EVENT_LOADEND, url, true, loaded_bytes,
                              total_bytes);
  DispatchProgressEvent(pp_instance_, loadend_event);

  // UMA
  HistogramEnumerateLoadStatus(PP_NACL_ERROR_LOAD_SUCCESS, is_installed_);
}

void NexeLoadManager::ReportLoadError(PP_NaClError error,
                                      const std::string& error_message) {
  ReportLoadError(error, error_message, error_message);
}

void NexeLoadManager::ReportLoadError(PP_NaClError error,
                                      const std::string& error_message,
                                      const std::string& console_message) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());

  if (error == PP_NACL_ERROR_MANIFEST_PROGRAM_MISSING_ARCH) {
    // A special case: the manifest may otherwise be valid but is missing
    // a program/file compatible with the user's sandbox.
    IPC::Sender* sender = content::RenderThread::Get();
    sender->Send(
        new NaClHostMsg_MissingArchError(GetRoutingID(pp_instance_)));
  }
  set_nacl_ready_state(PP_NACL_READY_STATE_DONE);
  nexe_error_reported_ = true;

  // We must set all properties before calling DispatchEvent so that when an
  // event handler runs, the properties reflect the current load state.
  std::string error_string = std::string("NaCl module load failed: ") +
      std::string(error_message);
  SetLastError(error_string);

  // Inform JavaScript that loading encountered an error and is complete.
  DispatchProgressEvent(pp_instance_, ProgressEvent(PP_NACL_EVENT_ERROR));
  DispatchProgressEvent(pp_instance_, ProgressEvent(PP_NACL_EVENT_LOADEND));

  HistogramEnumerateLoadStatus(error, is_installed_);
  LogToConsole(console_message);
}

void NexeLoadManager::ReportLoadAbort() {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());

  // Set the readyState attribute to indicate we need to start over.
  set_nacl_ready_state(PP_NACL_READY_STATE_DONE);
  nexe_error_reported_ = true;

  // Report an error in lastError and on the JavaScript console.
  std::string error_string("NaCl module load failed: user aborted");
  SetLastError(error_string);

  // Inform JavaScript that loading was aborted and is complete.
  DispatchProgressEvent(pp_instance_, ProgressEvent(PP_NACL_EVENT_ABORT));
  DispatchProgressEvent(pp_instance_, ProgressEvent(PP_NACL_EVENT_LOADEND));

  HistogramEnumerateLoadStatus(PP_NACL_ERROR_LOAD_ABORTED, is_installed_);
  LogToConsole(error_string);
}

void NexeLoadManager::NexeDidCrash() {
  VLOG(1) << "Plugin::NexeDidCrash: crash event!";
    // The NaCl module voluntarily exited.  However, this is still a
    // crash from the point of view of Pepper, since PPAPI plugins are
    // event handlers and should never exit.
  VLOG_IF(1, exit_status_ != -1)
      << "Plugin::NexeDidCrash: nexe exited with status " << exit_status_
      << " so this is a \"controlled crash\".";
  // If the crash occurs during load, we just want to report an error
  // that fits into our load progress event grammar.  If the crash
  // occurs after loaded/loadend, then we use ReportDeadNexe to send a
  // "crash" event.
  if (nexe_error_reported_) {
    VLOG(1) << "Plugin::NexeDidCrash: error already reported; suppressing";
  } else {
    if (nacl_ready_state_ == PP_NACL_READY_STATE_DONE) {
      ReportDeadNexe();
    } else {
      ReportLoadError(PP_NACL_ERROR_START_PROXY_CRASH,
                      "Nexe crashed during startup");
    }
  }
  // In all cases, try to grab the crash log.  The first error
  // reported may have come from the start_module RPC reply indicating
  // a validation error or something similar, which wouldn't grab the
  // crash log.  In the event that this is called twice, the second
  // invocation will just be a no-op, since the entire crash log will
  // have been received and we'll just get an EOF indication.

  base::ReadOnlySharedMemoryMapping shmem_mapping =
      crash_info_shmem_region_.MapAt(0, kNaClCrashInfoShmemSize);
  if (shmem_mapping.IsValid()) {
    base::BufferIterator<const uint8_t> buffer =
        shmem_mapping.GetMemoryAsBufferIterator<uint8_t>();
    const uint32_t* crash_log_length = buffer.Object<uint32_t>();
    base::span<const uint8_t> data = buffer.Span<uint8_t>(
        std::min<uint32_t>(*crash_log_length, kNaClCrashInfoMaxLogSize));
    std::string crash_log(data.begin(), data.end());
    CopyCrashLogToJsConsole(crash_log);
  }
}

void NexeLoadManager::set_trusted_plugin_channel(
    std::unique_ptr<TrustedPluginChannel> channel) {
  trusted_plugin_channel_ = std::move(channel);
}

void NexeLoadManager::set_manifest_service_channel(
    std::unique_ptr<ManifestServiceChannel> channel) {
  manifest_service_channel_ = std::move(channel);
}

PP_NaClReadyState NexeLoadManager::nacl_ready_state() {
  return nacl_ready_state_;
}

void NexeLoadManager::set_nacl_ready_state(PP_NaClReadyState ready_state) {
  nacl_ready_state_ = ready_state;
  ppapi::ScopedPPVar ready_state_name(
      ppapi::ScopedPPVar::PassRef(),
      ppapi::StringVar::StringToPPVar("readyState"));
  SetReadOnlyProperty(ready_state_name.get(), PP_MakeInt32(ready_state));
}

void NexeLoadManager::SetLastError(const std::string& error) {
  ppapi::ScopedPPVar error_name_var(
      ppapi::ScopedPPVar::PassRef(),
      ppapi::StringVar::StringToPPVar("lastError"));
  ppapi::ScopedPPVar error_var(
      ppapi::ScopedPPVar::PassRef(),
      ppapi::StringVar::StringToPPVar(error));
  SetReadOnlyProperty(error_name_var.get(), error_var.get());
}

void NexeLoadManager::SetReadOnlyProperty(PP_Var key, PP_Var value) {
  plugin_instance_->SetEmbedProperty(key, value);
}

void NexeLoadManager::LogToConsole(const std::string& message) {
  ppapi::PpapiGlobals::Get()->LogWithSource(
      pp_instance_, PP_LOGLEVEL_LOG, std::string("NativeClient"), message);
}

void NexeLoadManager::set_exit_status(int exit_status) {
  exit_status_ = exit_status;
  ppapi::ScopedPPVar exit_status_name_var(
      ppapi::ScopedPPVar::PassRef(),
      ppapi::StringVar::StringToPPVar("exitStatus"));
  SetReadOnlyProperty(exit_status_name_var.get(), PP_MakeInt32(exit_status));
}

void NexeLoadManager::InitializePlugin(
    uint32_t argc, const char* argn[], const char* argv[]) {
  init_time_ = base::Time::Now();

  for (size_t i = 0; i < argc; ++i) {
    std::string name(argn[i]);
    std::string value(argv[i]);
    args_[name] = value;
  }

  // Store mime_type_ at initialization time since we make it lowercase.
  mime_type_ = base::ToLowerASCII(LookupAttribute(args_, kTypeAttribute));
}

void NexeLoadManager::ReportStartupOverhead() const {
  base::TimeDelta overhead = base::Time::Now() - init_time_;
  HistogramStartupTimeMedium(
      "NaCl.Perf.StartupTime.NaClOverhead", overhead, nexe_size_);
}

bool NexeLoadManager::RequestNaClManifest(const std::string& url) {
  if (plugin_base_url_.is_valid()) {
    const GURL& resolved_url = plugin_base_url_.Resolve(url);
    if (resolved_url.is_valid()) {
      manifest_base_url_ = resolved_url;
      is_installed_ = manifest_base_url_.SchemeIs("chrome-extension");
      HistogramEnumerateManifestIsDataURI(
          manifest_base_url_.SchemeIs("data"));
      set_nacl_ready_state(PP_NACL_READY_STATE_OPENED);
      DispatchProgressEvent(pp_instance_,
                            ProgressEvent(PP_NACL_EVENT_LOADSTART));
      return true;
    }
  }
  ReportLoadError(PP_NACL_ERROR_MANIFEST_RESOLVE_URL,
                  std::string("could not resolve URL \"") + url +
                  "\" relative to \"" +
                  plugin_base_url_.possibly_invalid_spec() + "\".");
  return false;
}

void NexeLoadManager::ProcessNaClManifest(const std::string& program_url) {
  program_url_ = program_url;
  GURL gurl(program_url);
  DCHECK(gurl.is_valid());
  if (gurl.is_valid())
    is_installed_ = gurl.SchemeIs("chrome-extension");
  set_nacl_ready_state(PP_NACL_READY_STATE_LOADING);
  DispatchProgressEvent(pp_instance_, ProgressEvent(PP_NACL_EVENT_PROGRESS));
}

std::string NexeLoadManager::GetManifestURLArgument() const {
  std::string manifest_url;

  // If the MIME type is foreign, then this NEXE is being used as a content
  // type handler rather than directly by an HTML document.
  bool nexe_is_content_handler =
      !mime_type_.empty() &&
      mime_type_ != kNaClMIMEType &&
      mime_type_ != kPNaClMIMEType;

  if (nexe_is_content_handler) {
    // For content handlers 'src' will be the URL for the content
    // and 'nacl' will be the URL for the manifest.
    manifest_url = LookupAttribute(args_, kNaClManifestAttribute);
  } else {
    manifest_url = LookupAttribute(args_, kSrcManifestAttribute);
  }

  if (manifest_url.empty()) {
    VLOG(1) << "WARNING: no 'src' property, so no manifest loaded.";
    if (args_.find(kNaClManifestAttribute) != args_.end())
      VLOG(1) << "WARNING: 'nacl' property is incorrect. Use 'src'.";
  }
  return manifest_url;
}

void NexeLoadManager::CloseTrustedPluginChannel() {
  trusted_plugin_channel_.reset();
}

bool NexeLoadManager::IsPNaCl() const {
  return mime_type_ == kPNaClMIMEType;
}

void NexeLoadManager::ReportDeadNexe() {
  if (nacl_ready_state_ == PP_NACL_READY_STATE_DONE &&  // After loadEnd
      !nexe_error_reported_) {
    // Crashes will be more likely near startup, so use a medium histogram
    // instead of a large one.
    base::TimeDelta uptime = base::Time::Now() - ready_time_;
    HistogramTimeMedium("NaCl.ModuleUptime.Crash", uptime.InMilliseconds());

    std::string message("NaCl module crashed");
    SetLastError(message);
    LogToConsole(message);

    DispatchProgressEvent(pp_instance_, ProgressEvent(PP_NACL_EVENT_CRASH));
    nexe_error_reported_ = true;
  }
  // else ReportLoadError() and ReportLoadAbort() will be used by loading code
  // to provide error handling.
}

void NexeLoadManager::CopyCrashLogToJsConsole(const std::string& crash_log) {
  base::StringTokenizer t(crash_log, "\n");
  while (t.GetNext())
    LogToConsole(t.token());
}

}  // namespace nacl

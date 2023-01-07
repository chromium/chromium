// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/plugin/plugin.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "base/check.h"
#include "components/nacl/renderer/plugin/nacl_subprocess.h"
#include "components/nacl/renderer/plugin/plugin_error.h"
#include "components/nacl/renderer/plugin/service_runtime.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/module.h"

namespace {

void NoOpCallback(void* user_data, int32_t result) {
}

}

namespace plugin {

void Plugin::ShutDownSubprocesses() {
  // Shut down service runtime. This must be done before all other calls so
  // they don't block forever when waiting for the upcall thread to exit.
  main_subprocess_.Shutdown();
}

void Plugin::LoadNaClModule(PP_NaClFileInfo file_info,
                            PP_NaClAppProcessType process_type) {
  CHECK(pp::Module::Get()->core()->IsMainThread());
  // Before forking a new sel_ldr process, ensure that we do not leak
  // the ServiceRuntime object for an existing subprocess, and that any
  // associated listener threads do not go unjoined because if they
  // outlive the Plugin object, they will not be memory safe.
  ShutDownSubprocesses();
  pp::Var manifest_base_url =
      pp::Var(pp::PASS_REF,
              nacl::PPBNaClPrivate::GetManifestBaseURL(pp_instance()));
  std::string manifest_base_url_str = manifest_base_url.AsString();

  SelLdrStartParams params(manifest_base_url_str,
                           file_info,
                           process_type);
  ErrorInfo error_info;
  ServiceRuntime* service_runtime =
      new ServiceRuntime(this, pp_instance(), /*main_service_runtime=*/true);
  main_subprocess_.set_service_runtime(service_runtime);

  service_runtime->StartSelLdr(params,
                               pp::CompletionCallback(NoOpCallback, NULL));
}

void Plugin::LoadHelperNaClModule(const std::string& helper_url,
                                  PP_NaClFileInfo file_info,
                                  NaClSubprocess* subprocess_to_init,
                                  pp::CompletionCallback callback) {
  CHECK(pp::Module::Get()->core()->IsMainThread());
  // Do not report UMA stats for translator-related nexes.
  // TODO(sehr): define new UMA stats for translator related nexe events.
  SelLdrStartParams params(helper_url,
                           file_info,
                           PP_PNACL_TRANSLATOR_PROCESS_TYPE);
  ServiceRuntime* service_runtime =
      new ServiceRuntime(this, pp_instance(),
                         /*main_service_runtime=*/false);

  subprocess_to_init->set_service_runtime(service_runtime);
  service_runtime->StartSelLdr(params, callback);
}

// All failures of this function will show up as "Missing Plugin-in", so
// there is no need to log to JS console that there was an initialization
// failure. Note that module loading functions will log their own errors.
bool Plugin::Init(uint32_t argc, const char* argn[], const char* argv[]) {
  nacl::PPBNaClPrivate::InitializePlugin(pp_instance(), argc, argn, argv);
  pp::CompletionCallback open_cb =
      callback_factory_.NewCallback(&Plugin::NaClManifestFileDidOpen);
  nacl::PPBNaClPrivate::RequestNaClManifest(pp_instance(),
                                            open_cb.pp_completion_callback());
  return true;
}

Plugin::Plugin(PP_Instance pp_instance)
    : pp::Instance(pp_instance), uma_interface_(this) {
  callback_factory_.Initialize(this);

  // Notify PPBNaClPrivate that the instance is created before altering any
  // state that it tracks.
  nacl::PPBNaClPrivate::InstanceCreated(pp_instance);
  nexe_file_info_ = kInvalidNaClFileInfo;
}

Plugin::~Plugin() {
  // Destroy the coordinator while the rest of the data is still there
  pnacl_coordinator_.reset(NULL);

  nacl::PPBNaClPrivate::InstanceDestroyed(pp_instance());

  // ShutDownSubprocesses shuts down the main subprocess, which shuts
  // down the main ServiceRuntime object, which kills the subprocess.
  // As a side effect of the subprocess being killed, the reverse
  // services thread(s) will get EOF on the reverse channel(s), and
  // the thread(s) will exit.  In ServiceRuntime::Shutdown, we invoke
  // ReverseService::WaitForServiceThreadsToExit(), so that there will
  // not be an extent thread(s) hanging around.  This means that the
  // ~Plugin will block until this happens.  This is a requirement,
  // since the renderer should be free to unload the plugin code, and
  // we cannot have threads running code that gets unloaded before
  // they exit.
  //
  // By waiting for the threads here, we also ensure that the Plugin
  // object and the subprocess and ServiceRuntime objects is not
  // (fully) destroyed while the threads are running, so resources
  // that are destroyed after ShutDownSubprocesses (below) are
  // guaranteed to be live and valid for access from the service
  // threads.
  //
  // The main_subprocess object, which wraps the main service_runtime
  // object, is dtor'd implicitly after the explicit code below runs,
  // so the main service runtime object will not have been dtor'd,
  // though the Shutdown method may have been called, during the
  // lifetime of the service threads.
  ShutDownSubprocesses();
}

bool Plugin::HandleDocumentLoad(const pp::URLLoader& url_loader) {
  // We don't know if the plugin will handle the document load, but return
  // true in order to give it a chance to respond once the proxy is started.
  return true;
}

void Plugin::NexeFileDidOpen(int32_t pp_error) {
  if (pp_error != PP_OK)
    return;
  LoadNaClModule(nexe_file_info_, PP_NATIVE_NACL_PROCESS_TYPE);
}

void Plugin::BitcodeDidTranslate(int32_t pp_error) {
  if (pp_error != PP_OK) {
    // Error should have been reported by pnacl. Just return.
    return;
  }

  // Inform JavaScript that we successfully translated the bitcode to a nexe.
  PP_FileHandle handle = pnacl_coordinator_->TakeTranslatedFileHandle();

  PP_NaClFileInfo info;
  info.handle = handle;
  info.token_lo = 0;
  info.token_hi = 0;
  LoadNaClModule(info, PP_PNACL_PROCESS_TYPE);
}

void Plugin::NaClManifestFileDidOpen(int32_t pp_error) {
  if (pp_error != PP_OK)
    return;

  PP_Var pp_program_url;
  PP_PNaClOptions pnacl_options = {PP_FALSE, PP_FALSE, PP_FALSE, 2};
  if (nacl::PPBNaClPrivate::GetManifestProgramURL(
          pp_instance(), &pp_program_url, &pnacl_options)) {
    std::string program_url = pp::Var(pp::PASS_REF, pp_program_url).AsString();
    // TODO(teravest): Make ProcessNaClManifest take responsibility for more of
    // this function.
    nacl::PPBNaClPrivate::ProcessNaClManifest(pp_instance(),
                                              program_url.c_str());
    if (pnacl_options.translate) {
      pp::CompletionCallback translate_callback =
          callback_factory_.NewCallback(&Plugin::BitcodeDidTranslate);
      pnacl_coordinator_.reset(
          PnaclCoordinator::BitcodeToNative(this,
                                            program_url,
                                            pnacl_options,
                                            translate_callback));
      return;
    } else {
      pp::CompletionCallback open_callback =
          callback_factory_.NewCallback(&Plugin::NexeFileDidOpen);
      // Will always call the callback on success or failure.
      nacl::PPBNaClPrivate::DownloadNexe(
          pp_instance(),
          program_url.c_str(),
          &nexe_file_info_,
          open_callback.pp_completion_callback());
      return;
    }
  }
}

void Plugin::ReportLoadError(const ErrorInfo& error_info) {
  nacl::PPBNaClPrivate::ReportLoadError(pp_instance(),
                                        error_info.error_code(),
                                        error_info.message().c_str());
}

}  // namespace plugin

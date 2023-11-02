// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The portable representation of an instance and root scriptable object.
// The PPAPI version of the plugin instantiates a subclass of this class.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_PLUGIN_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_PLUGIN_H_

#include <stdint.h>
#include <stdio.h>

#include <memory>
#include <string>

#include "components/nacl/renderer/plugin/nacl_subprocess.h"
#include "components/nacl/renderer/plugin/pnacl_coordinator.h"
#include "components/nacl/renderer/plugin/service_runtime.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/private/uma_private.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/view.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace pp {
class CompletionCallback;
class URLLoader;
}

namespace plugin {

class ErrorInfo;

const PP_NaClFileInfo kInvalidNaClFileInfo = {
  PP_kInvalidFileHandle,
  0,  // token_lo
  0,  // token_hi
};

class Plugin : public pp::Instance {
 public:
  explicit Plugin(PP_Instance instance);

  Plugin(const Plugin&) = delete;
  Plugin& operator=(const Plugin&) = delete;

  // ----- Methods inherited from pp::Instance:

  // Initializes this plugin with <embed/object ...> tag attribute count |argc|,
  // names |argn| and values |argn|. Returns false on failure.
  // Gets called by the browser right after New().
  bool Init(uint32_t argc, const char* argn[], const char* argv[]) override;

  // Handles document load, when the plugin is a MIME type handler.
  bool HandleDocumentLoad(const pp::URLLoader& url_loader) override;

  // Load support.
  //
  // Starts NaCl module but does not wait until low-level
  // initialization (e.g. ld.so dynamic loading of manifest files) is
  // done.  The module will become ready later, asynchronously.  Other
  // event handlers should block until the module is ready before
  // trying to communicate with it, i.e., until nacl_ready_state is
  // DONE.
  //
  // NB: currently we do not time out, so if the untrusted code
  // does not signal that it is ready, then we will deadlock the main
  // thread of the renderer on this subsequent event delivery.  We
  // should include a time-out at which point we declare the
  // nacl_ready_state to be done, and let the normal crash detection
  // mechanism(s) take over.
  // This function takes over ownership of the file_info.
  void LoadNaClModule(PP_NaClFileInfo file_info,
                      PP_NaClAppProcessType process_type);

  // Load support.
  // A helper SRPC NaCl module can be loaded given a PP_NaClFileInfo.
  // Does not update nacl_module_origin().
  // Uses the given NaClSubprocess to contain the new SelLdr process.
  // The given callback is called when the loading is complete.
  // This function takes over ownership of the file_info.
  void LoadHelperNaClModule(const std::string& helper_url,
                            PP_NaClFileInfo file_info,
                            NaClSubprocess* subprocess_to_init,
                            pp::CompletionCallback callback);

  // Report an error that was encountered while loading a module.
  void ReportLoadError(const ErrorInfo& error_info);

 private:
  // The browser will invoke the destructor via the pp::Instance
  // pointer to this object, not from base's Delete().
  ~Plugin() override;

  // Shuts down socket connection, service runtime, and receive thread,
  // in this order, for the main nacl subprocess.
  void ShutDownSubprocesses();

  // Callback used when getting the URL for the .nexe file.  If the URL loading
  // is successful, the file descriptor is opened and can be passed to sel_ldr
  // with the sandbox on.
  void NexeFileDidOpen(int32_t pp_error);

  // Callback used when a .nexe is translated from bitcode.  If the translation
  // is successful, the file descriptor is opened and can be passed to sel_ldr
  // with the sandbox on.
  void BitcodeDidTranslate(int32_t pp_error);

  // NaCl ISA selection manifest file support.  The manifest file is specified
  // using the "nacl" attribute in the <embed> tag.  First, the manifest URL (or
  // data: URI) is fetched, then the JSON is parsed.  Once a valid .nexe is
  // chosen for the sandbox ISA, any current service runtime is shut down, the
  // .nexe is loaded and run.

  // Callback used when getting the manifest file as a local file descriptor.
  void NaClManifestFileDidOpen(int32_t pp_error);

  // Processes the JSON manifest string and starts loading the nexe.
  void ProcessNaClManifest(const std::string& manifest_json);

  // Keep track of the NaCl module subprocess that was spun up in the plugin.
  NaClSubprocess main_subprocess_;

  pp::CompletionCallbackFactory<Plugin> callback_factory_;

  std::unique_ptr<PnaclCoordinator> pnacl_coordinator_;

  int exit_status_;

  PP_NaClFileInfo nexe_file_info_;

  pp::UMAPrivate uma_interface_;
};

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_PLUGIN_H_

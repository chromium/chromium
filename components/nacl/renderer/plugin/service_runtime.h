// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class containing information regarding a socket connection to a
// service runtime instance.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_SERVICE_RUNTIME_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_SERVICE_RUNTIME_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/cpp/completion_callback.h"

namespace plugin {

class Plugin;
class ServiceRuntime;

// Struct of params used by StartSelLdr.  Use a struct so that callback
// creation templates aren't overwhelmed with too many parameters.
struct SelLdrStartParams {
  SelLdrStartParams(const std::string& url,
                    const PP_NaClFileInfo& file_info,
                    PP_NaClAppProcessType process_type)
      : url(url),
        file_info(file_info),
        process_type(process_type) {
  }
  std::string url;
  PP_NaClFileInfo file_info;
  PP_NaClAppProcessType process_type;
};

// ServiceRuntime abstracts a NativeClient sel_ldr instance.
// TODO(dschuff): Merge this with NaClSubprocess, since, that now only contains
// a ServiceRuntime.
class ServiceRuntime {
 public:
  ServiceRuntime(Plugin* plugin,
                 PP_Instance pp_instance,
                 bool main_service_runtime);

  ServiceRuntime(const ServiceRuntime&) = delete;
  ServiceRuntime& operator=(const ServiceRuntime&) = delete;

  // The destructor terminates the sel_ldr process.
  ~ServiceRuntime();

  // Spawn the sel_ldr instance.
  void StartSelLdr(const SelLdrStartParams& params,
                   pp::CompletionCallback callback);

  Plugin* plugin() const { return plugin_; }
  void Shutdown();

  bool main_service_runtime() const { return main_service_runtime_; }

  std::unique_ptr<IPC::SyncChannel> TakeTranslatorChannel() {
    return std::unique_ptr<IPC::SyncChannel>(translator_channel_.release());
  }

 private:
  raw_ptr<Plugin> plugin_;
  PP_Instance pp_instance_;
  bool main_service_runtime_;

  std::unique_ptr<IPC::SyncChannel> translator_channel_;
};

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_SERVICE_RUNTIME_H_

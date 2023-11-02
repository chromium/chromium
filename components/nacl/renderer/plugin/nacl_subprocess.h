// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Instances of NaCl modules spun up within the plugin as a subprocess.
// This may represent the "main" nacl module, or it may represent helpers
// that perform various tasks within the plugin, for example,
// a NaCl module for a compiler could be loaded to translate LLVM bitcode
// into native code.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_NACL_SUBPROCESS_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_NACL_SUBPROCESS_H_

#include <stdarg.h>

#include <memory>

#include "components/nacl/renderer/plugin/service_runtime.h"

namespace plugin {

class ServiceRuntime;


// A class representing an instance of a NaCl module, loaded by the plugin.
class NaClSubprocess {
 public:
  NaClSubprocess();

  NaClSubprocess(const NaClSubprocess&) = delete;
  NaClSubprocess& operator=(const NaClSubprocess&) = delete;

  virtual ~NaClSubprocess();

  ServiceRuntime* service_runtime() const { return service_runtime_.get(); }
  void set_service_runtime(ServiceRuntime* service_runtime) {
    service_runtime_.reset(service_runtime);
  }

  // Fully shut down the subprocess.
  void Shutdown();

 private:
  // The service runtime representing the NaCl module instance.
  std::unique_ptr<ServiceRuntime> service_runtime_;
};

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_NACL_SUBPROCESS_H_

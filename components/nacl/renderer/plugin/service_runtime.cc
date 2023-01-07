// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/plugin/service_runtime.h"

#include <string.h>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "components/nacl/renderer/plugin/plugin.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/core.h"

namespace plugin {

ServiceRuntime::ServiceRuntime(Plugin* plugin,
                               PP_Instance pp_instance,
                               bool main_service_runtime)
    : plugin_(plugin),
      pp_instance_(pp_instance),
      main_service_runtime_(main_service_runtime) {}

void ServiceRuntime::StartSelLdr(const SelLdrStartParams& params,
                                 pp::CompletionCallback callback) {
  nacl::PPBNaClPrivate::LaunchSelLdr(
      pp_instance_, PP_FromBool(main_service_runtime_), params.url.c_str(),
      &params.file_info, params.process_type, &translator_channel_,
      callback.pp_completion_callback());
}

void ServiceRuntime::Shutdown() {
  nacl::PPBNaClPrivate::TerminateNaClLoader(pp_instance_);
}

ServiceRuntime::~ServiceRuntime() {
  Shutdown();
}

}  // namespace plugin

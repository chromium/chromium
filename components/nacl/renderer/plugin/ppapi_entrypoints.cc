// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/plugin/ppapi_entrypoints.h"

#include <stdint.h>

#include "components/nacl/renderer/plugin/module_ppapi.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/internal_module.h"

namespace nacl_plugin {

int32_t PPP_InitializeModule(PP_Module module_id,
                             PPB_GetInterface get_browser_interface) {
  plugin::ModulePpapi* module = new plugin::ModulePpapi();
  if (!module->InternalInit(module_id, get_browser_interface)) {
    delete module;
    return PP_ERROR_FAILED;
  }

  pp::InternalSetModuleSingleton(module);
  return PP_OK;
}

void PPP_ShutdownModule() {
  delete pp::Module::Get();
  pp::InternalSetModuleSingleton(NULL);
}

const void* PPP_GetInterface(const char* interface_name) {
  if (!pp::Module::Get())
    return NULL;
  return pp::Module::Get()->GetPluginInterface(interface_name);
}

}  // namespace nacl_plugin

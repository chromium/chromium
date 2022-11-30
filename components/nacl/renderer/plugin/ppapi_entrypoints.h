// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_PPAPI_ENTRYPOINTS_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_PPAPI_ENTRYPOINTS_H_

#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"

// Provides entry points for the trusted plugin.
namespace nacl_plugin {

int PPP_InitializeModule(PP_Module module,
                         PPB_GetInterface get_browser_interface);
const void* PPP_GetInterface(const char* interface_name);
void PPP_ShutdownModule();

}  // namespace nacl_plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_PPAPI_ENTRYPOINTS_H_

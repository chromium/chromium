// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppp_instance.h"

#include "ppapi/c/ppp.h"

int32_t PPP_InitializeModule(PP_Module module_id,
                             PPB_GetInterface get_browser_interface) {
  printf("PPP_InitializeModule\n");

  // Request an unsupported interface.
  CHECK(NULL == get_browser_interface("UnsupportedInterface;1.0"));
  // Request a supported interface with a bad revision number.
  CHECK(NULL == get_browser_interface("PPB_Instance;0.0"));

  return PP_OK;
}

PP_EXPORT void PPP_ShutdownModule() {
  printf("PPP_ShutdownModule\n");
  fflush(stdout);
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  printf("PPP_GetInterface(%s)\n", interface_name);
  if (0 == std::strcmp(interface_name, PPP_INSTANCE_INTERFACE))  // Required.
    return NULL;
  return NULL;
}

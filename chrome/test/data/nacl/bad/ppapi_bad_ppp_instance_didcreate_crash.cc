// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"

PP_EXPORT int32_t PPP_InitializeModule(PP_Module module_id,
                                       PPB_GetInterface get_browser_interface) {
  printf("PPP_InitializeModule\n");
  return PP_OK;
}

PP_EXPORT void PPP_ShutdownModule() {
  printf("PPP_ShutdownModule\n");
  fflush(stdout);
}

namespace {

PP_Bool DidCreate(PP_Instance /*instance*/,
                  uint32_t /*argc*/,
                  const char* /*argn*/[],
                  const char* /*argv*/[]) {
  CRASH;
  return PP_FALSE;
}

const PPP_Instance instance_interface = {
  DidCreate,
  // Normally one is not alowed to register NULLs for functions, but
  // since DidCreate is going to crash, these will never be called.
  NULL,
  NULL,
  NULL,
  NULL
};

}  // namespace

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  printf("PPP_GetInterface(%s)\n", interface_name);
  if (0 == std::strcmp(interface_name, PPP_INSTANCE_INTERFACE))  // Required.
    return reinterpret_cast<const void*>(&instance_interface);
  return NULL;
}

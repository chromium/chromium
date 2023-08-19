// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppp.h"

PP_EXPORT int32_t PPP_InitializeModule(PP_Module module_id,
                                       PPB_GetInterface get_browser_interface) {
  printf("PPP_InitializeModule\n");
  return PP_ERROR_FAILED;
}

PP_EXPORT void PPP_ShutdownModule() {
  printf("PPP_ShutdownModule\n");
  NACL_NOTREACHED();  // If initialization fails, this should not be called.
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  printf("PPP_GetInterface\n");
  NACL_NOTREACHED();  // If initialization fails, this should not be called.
}

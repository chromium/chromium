// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/nacl/ppapi_test_lib/internal_utils.h"

namespace {

PP_Module global_pp_module = 0;
PP_Instance global_pp_instance = 0;
PPB_GetInterface global_ppb_get_interface = NULL;

}  // namespace

void set_ppb_get_interface(PPB_GetInterface get_interface) {
  global_ppb_get_interface = get_interface;
}
void set_pp_instance(PP_Instance instance) { global_pp_instance = instance; }
void set_pp_module(PP_Module module) { global_pp_module = module; }
PPB_GetInterface ppb_get_interface() { return global_ppb_get_interface; }
PP_Module pp_module() { return global_pp_module; }
PP_Instance pp_instance() { return global_pp_instance; }

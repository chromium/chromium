// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/plugin/module_ppapi.h"
#include "components/nacl/renderer/plugin/plugin.h"

namespace plugin {

ModulePpapi::ModulePpapi() : pp::Module() {
}

ModulePpapi::~ModulePpapi() {
}

bool ModulePpapi::Init() {
  return true;
}

pp::Instance* ModulePpapi::CreateInstance(PP_Instance pp_instance) {
  return new Plugin(pp_instance);
}

}  // namespace plugin


namespace pp {

Module* CreateModule() {
  return new plugin::ModulePpapi();
}

}  // namespace pp

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_MODULE_PPAPI_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_MODULE_PPAPI_H_

#include "ppapi/cpp/module.h"

namespace plugin {

class ModulePpapi : public pp::Module {
 public:
  ModulePpapi();

  ~ModulePpapi() override;

  bool Init() override;

  pp::Instance* CreateInstance(PP_Instance pp_instance) override;
};

}  // namespace plugin


namespace pp {

Module* CreateModule();

}  // namespace pp

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_MODULE_PPAPI_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

class SimpleInstance : public pp::Instance {
 public:
  explicit SimpleInstance(PP_Instance instance) : pp::Instance(instance) {
  }

  virtual void HandleMessage(const pp::Var& var_message) {
    if (var_message.is_string() && var_message.AsString() == "ping") {
      PostMessage(pp::Var("pong"));
      return;
    }
    PostMessage(pp::Var("failed"));
  }
};

class SimpleModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new SimpleInstance(instance);
  }
};

namespace pp {

__attribute__((visibility("default")))
Module* CreateModule() {
  return new SimpleModule();
}

}  // namespace pp

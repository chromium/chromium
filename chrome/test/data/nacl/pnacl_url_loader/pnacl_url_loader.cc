// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

class PnaclUrlLoaderInstance : public pp::Instance {
 public:
  explicit PnaclUrlLoaderInstance(PP_Instance instance)
      : pp::Instance(instance), loader_(this), factory_(this) {}

  void HandleMessage(const pp::Var& var_message) override {
    if (var_message.is_string()) {
      command_ = var_message.AsString();
      pp::URLRequestInfo request(this);
      request.SetMethod("GET");
      request.SetURL("/echo");
      loader_.Open(request,
                   factory_.NewCallback(&PnaclUrlLoaderInstance::OnOpen));
      return;
    }
  }

 private:
  void OnOpen(int32_t result) { PostMessage(pp::Var("OnOpen" + command_)); }

  pp::URLLoader loader_;
  pp::CompletionCallbackFactory<PnaclUrlLoaderInstance> factory_;
  std::string command_;
};

class PnaclUrlLoaderModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new PnaclUrlLoaderInstance(instance);
  }
};

namespace pp {

__attribute__((visibility("default"))) Module* CreateModule() {
  return new PnaclUrlLoaderModule();
}

}  // namespace pp

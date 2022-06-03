// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_PRINTING_HOOKS_DELEGATE_H_
#define CHROME_RENDERER_EXTENSIONS_PRINTING_HOOKS_DELEGATE_H_

#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8.h"

namespace extensions {

// Custom native hooks for the printing API.
class PrintingHooksDelegate : public APIBindingHooksDelegate {
 public:
  PrintingHooksDelegate();
  ~PrintingHooksDelegate() override;

  PrintingHooksDelegate(const PrintingHooksDelegate&) = delete;
  PrintingHooksDelegate& operator=(const PrintingHooksDelegate&) = delete;

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      std::vector<v8::Local<v8::Value>>* arguments,
      const APITypeReferenceMap& refs) override;

 private:
  APIBindingHooks::RequestResult HandleSubmitJob(
      v8::Isolate* isolate,
      std::vector<v8::Local<v8::Value>>* arguments);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_PRINTING_HOOKS_DELEGATE_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_ACCESSIBILITY_PRIVATE_HOOKS_DELEGATE_H_
#define CHROME_RENDERER_EXTENSIONS_ACCESSIBILITY_PRIVATE_HOOKS_DELEGATE_H_

#include <vector>

#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8.h"

namespace extensions {
class ScriptContext;

// Custom native hooks for the accessibilityPrivate API.
// Methods are implemented as handlers in the implementation file
// e.g. Handle<FunctionName>.
// Handlers are implemented for synchronous methods of the API.
class AccessibilityPrivateHooksDelegate : public APIBindingHooksDelegate {
 public:
  AccessibilityPrivateHooksDelegate();
  ~AccessibilityPrivateHooksDelegate() override;

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      std::vector<v8::Local<v8::Value>>* arguments,
      const APITypeReferenceMap& refs) override;

 private:
  // Method handlers:
  APIBindingHooks::RequestResult HandleGetDisplayLanguage(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& parsed_arguments);
  DISALLOW_COPY_AND_ASSIGN(AccessibilityPrivateHooksDelegate);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_ACCESSIBILITY_PRIVATE_HOOKS_DELEGATE_H_

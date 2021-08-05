// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_HOOKS_DELEGATE_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_HOOKS_DELEGATE_H_

#include <vector>

#include "base/macros.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8.h"

namespace extensions {
class NativeRendererMessagingService;
class ScriptContext;

// The custom hooks for the chrome.extension API.
class ExtensionHooksDelegate : public APIBindingHooksDelegate {
 public:
  explicit ExtensionHooksDelegate(
      NativeRendererMessagingService* messaging_service);
  ~ExtensionHooksDelegate() override;

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      std::vector<v8::Local<v8::Value>>* arguments,
      const APITypeReferenceMap& refs) override;
  void InitializeTemplate(v8::Isolate* isolate,
                          v8::Local<v8::ObjectTemplate> object_template,
                          const APITypeReferenceMap& type_refs) override;
  void InitializeInstance(v8::Local<v8::Context> context,
                          v8::Local<v8::Object> instance) override;

 private:
  // Request handlers for the corresponding API methods.
  APIBindingHooks::RequestResult HandleSendRequest(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);
  APIBindingHooks::RequestResult HandleGetURL(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);
  APIBindingHooks::RequestResult HandleGetViews(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);
  APIBindingHooks::RequestResult HandleGetExtensionTabs(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);
  APIBindingHooks::RequestResult HandleGetBackgroundPage(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);

  // The messaging service to handle messaging calls.
  // Guaranteed to outlive this object.
  NativeRendererMessagingService* const messaging_service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHooksDelegate);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_HOOKS_DELEGATE_H_

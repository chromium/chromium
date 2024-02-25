// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_API_EXTENSION_HOOKS_DELEGATE_H_
#define CHROME_RENDERER_EXTENSIONS_API_EXTENSION_HOOKS_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "v8/include/v8.h"

namespace extensions {
class NativeRendererMessagingService;
class ScriptContext;

// The custom hooks for the chrome.extension API.
class ExtensionHooksDelegate : public APIBindingHooksDelegate {
 public:
  explicit ExtensionHooksDelegate(
      NativeRendererMessagingService* messaging_service);

  ExtensionHooksDelegate(const ExtensionHooksDelegate&) = delete;
  ExtensionHooksDelegate& operator=(const ExtensionHooksDelegate&) = delete;

  ~ExtensionHooksDelegate() override;

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>* arguments,
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
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleGetURL(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleGetViews(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleGetExtensionTabs(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleGetBackgroundPage(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);

  // The messaging service to handle messaging calls.
  // Guaranteed to outlive this object.
  const raw_ptr<NativeRendererMessagingService, DanglingUntriaged>
      messaging_service_;
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_API_EXTENSION_HOOKS_DELEGATE_H_

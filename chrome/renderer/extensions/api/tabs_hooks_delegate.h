// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_API_TABS_HOOKS_DELEGATE_H_
#define CHROME_RENDERER_EXTENSIONS_API_TABS_HOOKS_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "v8/include/v8.h"

namespace extensions {
class NativeRendererMessagingService;
class ScriptContext;

// The custom hooks for the tabs API.
class TabsHooksDelegate : public APIBindingHooksDelegate {
 public:
  explicit TabsHooksDelegate(NativeRendererMessagingService* messaging_service);

  TabsHooksDelegate(const TabsHooksDelegate&) = delete;
  TabsHooksDelegate& operator=(const TabsHooksDelegate&) = delete;

  ~TabsHooksDelegate() override;

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>* arguments,
      const APITypeReferenceMap& refs) override;

 private:
  // Request handlers for the corresponding API methods.
  APIBindingHooks::RequestResult HandleSendRequest(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleSendMessage(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleConnect(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);

  // The messaging service to handle connect() and sendMessage() calls.
  // Guaranteed to outlive this object.
  const raw_ptr<NativeRendererMessagingService, DanglingUntriaged>
      messaging_service_;
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_API_TABS_HOOKS_DELEGATE_H_

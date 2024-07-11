// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/identity_hooks_delegate.h"

#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

using IdentityHooksDelegateTest = NativeExtensionBindingsSystemUnittest;

// Tests that the result modifier used in the getAuthToken handle request hook
// results in callback-based calls getting a response with multiple arguments
// and promise-based calls getting a response with a single object.
TEST_F(IdentityHooksDelegateTest, GetAuthToken) {
  // Initialize bindings system.
  bindings_system()->api_system()->RegisterHooksDelegate(
      "identity", std::make_unique<IdentityHooksDelegate>());
  // Register extension.
  scoped_refptr<const Extension> extension = ExtensionBuilder("testExtension")
                                                 .SetManifestVersion(3)
                                                 .AddAPIPermission("identity")
                                                 .Build();
  RegisterExtension(extension);
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(script_context);

  constexpr char kFakeAPIResponse[] =
      R"([{"token": "foo", "grantedScopes": ["bar"]}])";

  // Calling getAuthToken without a callback should return a promise that gets
  // fulfilled with an object with the results as properties on it.
  {
    v8::Local<v8::Function> func = FunctionFromString(
        context, "(function() { return chrome.identity.getAuthToken(); })");
    v8::Local<v8::Value> result = RunFunction(func, context, 0, nullptr);
    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(result, &promise));
    EXPECT_EQ(v8::Promise::kPending, promise->State());

    bindings_system()->HandleResponse(last_params().request_id,
                                      /*success=*/true,
                                      ListValueFromString(kFakeAPIResponse),
                                      /*error=*/std::string());

    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    // Note that the object here differs slightly from the response, in that it
    // is not array wrapped and the keys become alphabetized.
    EXPECT_EQ(R"({"grantedScopes":["bar"],"token":"foo"})",
              V8ToString(promise->Result(), context));
  }

  // Calling getAuthToken with a callback should end up with the callback being
  // called with multiple parameters rather than a single object.
  {
    constexpr char kFunctionCall[] =
        R"((function(api) {
             chrome.identity.getAuthToken((token, grantedScopes) => {
               this.argument1 = token;
               this.argument2 = grantedScopes;
             });
           }))";
    v8::Local<v8::Function> func = FunctionFromString(context, kFunctionCall);
    RunFunctionOnGlobal(func, context, 0, nullptr);

    bindings_system()->HandleResponse(last_params().request_id,
                                      /*success=*/true,
                                      ListValueFromString(kFakeAPIResponse),
                                      /*error=*/std::string());

    EXPECT_EQ(R"("foo")", GetStringPropertyFromObject(context->Global(),
                                                      context, "argument1"));
    EXPECT_EQ(R"(["bar"])", GetStringPropertyFromObject(context->Global(),
                                                        context, "argument2"));
  }
}

}  // namespace extensions

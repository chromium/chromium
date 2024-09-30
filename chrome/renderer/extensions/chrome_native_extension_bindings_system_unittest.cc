// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

namespace {

constexpr char kCallUserScriptsRegister[] =
    R"((function() {
         chrome.userScripts.register(
             [{
                id: 'script',
                matches: ['*://*/*'],
                js: [{file: 'script.js'}],
             }]);
       });)";

}  // namespace

TEST_F(NativeExtensionBindingsSystemUnittest, InitializeContext) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .AddAPIPermissions({"idle", "power", "webRequest", "tabs"})
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // Test that chrome-specified APIs (like tabs) are present.
  v8::Local<v8::Value> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome");
  ASSERT_FALSE(chrome.IsEmpty());
  ASSERT_TRUE(chrome->IsObject());

  v8::Local<v8::Value> tabs =
      GetPropertyFromObject(chrome.As<v8::Object>(), context, "tabs");
  ASSERT_FALSE(tabs.IsEmpty());
  ASSERT_TRUE(tabs->IsObject());

  v8::Local<v8::Value> query =
      GetPropertyFromObject(tabs.As<v8::Object>(), context, "query");
  ASSERT_FALSE(query.IsEmpty());
  ASSERT_TRUE(query->IsFunction());
}

TEST_F(NativeExtensionBindingsSystemUnittest,
       RestrictDeveloperModeAPIsUserIsInDeveloperMode) {

  // With kDeveloperModeRestriction enabled, developer mode-only APIs
  // should be available if and only if the user is in dev mode.
  SetCurrentDeveloperMode(kRendererProfileId, true);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .AddAPIPermission("userScripts")
          .SetManifestVersion(3)
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // chrome.userScripts.getTargets should exist.
  v8::Local<v8::Value> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome");
  ASSERT_FALSE(chrome.IsEmpty());
  ASSERT_TRUE(chrome->IsObject());

  v8::Local<v8::Value> user_scripts_api = GetPropertyFromObject(
      v8::Local<v8::Object>::Cast(chrome), context, "userScripts");
  ASSERT_FALSE(user_scripts_api.IsEmpty());
  ASSERT_TRUE(user_scripts_api->IsObject());

  v8::Local<v8::Object> user_scripts_object =
      v8::Local<v8::Object>::Cast(user_scripts_api);
  v8::Local<v8::Value> user_scripts_register =
      GetPropertyFromObject(user_scripts_object, context, "register");
  ASSERT_FALSE(user_scripts_register.IsEmpty());

  {
    v8::Local<v8::Function> call_register =
        FunctionFromString(context, kCallUserScriptsRegister);
    RunFunctionOnGlobal(call_register, context, 0, nullptr);
  }

  // Validate the params that would be sent to the browser.
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("userScripts.register", last_params().name);
  EXPECT_EQ(extension->url(), last_params().source_url);
  EXPECT_TRUE(last_params().has_callback);
  // No need to look at the full arguments, but sanity check their general
  // shape.
  EXPECT_EQ(1u, last_params().arguments.size());
  EXPECT_EQ(base::Value::Type::LIST, last_params().arguments[0].type());
}

TEST_F(NativeExtensionBindingsSystemUnittest,
       RestrictDeveloperModeAPIsUserIsNotInDeveloperModeAndHasPermission) {

  // With kDeveloperModeRestriction enabled, developer mode-only APIs
  // should not be available if the user is not in dev mode.
  SetCurrentDeveloperMode(kRendererProfileId, false);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .AddAPIPermission("userScripts")
          .SetManifestVersion(3)
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome");
  ASSERT_FALSE(chrome.IsEmpty());
  ASSERT_TRUE(chrome->IsObject());

  {
    v8::Local<v8::Function> call_user_scripts_register =
        FunctionFromString(context, kCallUserScriptsRegister);
    RunFunctionAndExpectError(call_user_scripts_register, context, 0, nullptr,
                              "Uncaught Error: The 'userScripts' API is only "
                              "available for users in developer mode.");
  }
}

TEST_F(
    NativeExtensionBindingsSystemUnittest,
    RestrictDeveloperModeAPIsUserIsNotInDeveloperModeAndDoesNotHavePermission) {

  SetCurrentDeveloperMode(kRendererProfileId, false);

  scoped_refptr<const Extension> extension = ExtensionBuilder("foo").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome");
  ASSERT_FALSE(chrome.IsEmpty());
  ASSERT_TRUE(chrome->IsObject());

  v8::Local<v8::Value> user_scripts = GetPropertyFromObject(
      v8::Local<v8::Object>::Cast(chrome), context, "userScripts");
  ASSERT_FALSE(user_scripts.IsEmpty());
  EXPECT_TRUE(user_scripts->IsUndefined());
}

}  // namespace extensions

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

TEST_F(NativeExtensionBindingsSystemUnittest, InitializeContext) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .AddPermissions({"idle", "power", "webRequest", "tabs"})
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kRestrictDeveloperModeAPIs);

  // With kDeveloperModeRestriction enabled, developer mode-only APIs
  // should be available if and only if the user is in dev mode.
  SetCurrentDeveloperMode(kRendererProfileId, true);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddPermissions({"debugger"}).Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // chrome.debugger.getTargets should exist.
  v8::Local<v8::Value> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome");
  ASSERT_FALSE(chrome.IsEmpty());
  ASSERT_TRUE(chrome->IsObject());

  v8::Local<v8::Value> debugger = GetPropertyFromObject(
      v8::Local<v8::Object>::Cast(chrome), context, "debugger");
  ASSERT_FALSE(debugger.IsEmpty());
  ASSERT_TRUE(debugger->IsObject());

  v8::Local<v8::Object> debugger_object = v8::Local<v8::Object>::Cast(debugger);
  v8::Local<v8::Value> debugger_getTargets =
      GetPropertyFromObject(debugger_object, context, "getTargets");
  ASSERT_FALSE(debugger_getTargets.IsEmpty());

  {
    // Call the function correctly.
    const char kCallDebuggerGetTargets[] =
        R"((function() {
          chrome.debugger.getTargets(function() {});
        });)";

    v8::Local<v8::Function> call_debugger_getTargets =
        FunctionFromString(context, kCallDebuggerGetTargets);
    RunFunctionOnGlobal(call_debugger_getTargets, context, 0, nullptr);
  }

  // Validate the params that would be sent to the browser.
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("debugger.getTargets", last_params().name);
  EXPECT_EQ(extension->url(), last_params().source_url);
  EXPECT_TRUE(last_params().has_callback);
  EXPECT_EQ(last_params().arguments, ListValueFromString("[ ]"));
}

TEST_F(NativeExtensionBindingsSystemUnittest,
       RestrictDeveloperModeAPIsUserIsNotInDeveloperModeAndHasPermission) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kRestrictDeveloperModeAPIs);

  // With kDeveloperModeRestriction enabled, developer mode-only APIs
  // should not be available if the user is not in dev mode.
  SetCurrentDeveloperMode(kRendererProfileId, false);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddPermissions({"debugger"}).Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome");
  ASSERT_FALSE(chrome.IsEmpty());
  ASSERT_TRUE(chrome->IsObject());

  {
    const char kCallDebuggerGetTargets[] =
        R"((function() {
          chrome.debugger(function() {});
        });)";

    v8::Local<v8::Function> call_debugger_getTargets =
        FunctionFromString(context, kCallDebuggerGetTargets);
    RunFunctionAndExpectError(call_debugger_getTargets, context, 0, nullptr,
                              "Uncaught Error: The 'debugger' API is only "
                              "available for users in developer mode.");
  }
}

TEST_F(
    NativeExtensionBindingsSystemUnittest,
    RestrictDeveloperModeAPIsUserIsNotInDeveloperModeAndDoesNotHavePermission) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kRestrictDeveloperModeAPIs);

  SetCurrentDeveloperMode(kRendererProfileId, false);

  scoped_refptr<const Extension> extension = ExtensionBuilder("foo").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome");
  ASSERT_FALSE(chrome.IsEmpty());
  ASSERT_TRUE(chrome->IsObject());

  v8::Local<v8::Value> debugger = GetPropertyFromObject(
      v8::Local<v8::Object>::Cast(chrome), context, "debugger");
  ASSERT_FALSE(debugger.IsEmpty());
  EXPECT_TRUE(debugger->IsUndefined());
}

}  // namespace extensions

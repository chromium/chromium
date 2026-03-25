// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

namespace {

constexpr char kCallDebuggerGetTargets[] =
    R"((function() {
         chrome.debugger.getTargets();
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

// Tests that Developer Mode controls API visibility.
class DeveloperModeBindingsSystemUnittest
    : public NativeExtensionBindingsSystemUnittest {
 public:
  DeveloperModeBindingsSystemUnittest() {
    // Ensure chrome.debugger is controlled by Developer Mode.
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kDebuggerAPIRestrictedToDevMode);
  }

  DeveloperModeBindingsSystemUnittest(
      const DeveloperModeBindingsSystemUnittest&) = delete;
  DeveloperModeBindingsSystemUnittest& operator=(
      const DeveloperModeBindingsSystemUnittest&) = delete;
  ~DeveloperModeBindingsSystemUnittest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DeveloperModeBindingsSystemUnittest,
       RestrictDeveloperModeAPIsUserIsInDeveloperMode) {
  // With kDeveloperModeRestriction enabled, developer mode-only APIs
  // should be available if and only if the user is in dev mode.
  SetCurrentDeveloperMode(kRendererProfileId, true);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermission("debugger").Build();
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
  v8::Local<v8::Value> api = GetPropertyFromObject(
      v8::Local<v8::Object>::Cast(chrome), context, "debugger");
  ASSERT_FALSE(api.IsEmpty());
  ASSERT_TRUE(api->IsObject());

  // `debugger.getTargets` should exist.
  v8::Local<v8::Object> api_object = v8::Local<v8::Object>::Cast(api);
  v8::Local<v8::Value> api_method_call =
      GetPropertyFromObject(api_object, context, "getTargets");
  ASSERT_FALSE(api_method_call.IsEmpty());

  {
    v8::Local<v8::Function> call_api_method =
        FunctionFromString(context, kCallDebuggerGetTargets);
    RunFunctionOnGlobal(call_api_method, context, 0, nullptr);
  }

  // Validate the params that would be sent to the browser.
  EXPECT_TRUE(last_params().has_callback);
  EXPECT_EQ("debugger.getTargets", last_params().name);
  // No need to look at the full arguments, but sanity check their general
  // shape.
  EXPECT_EQ(0u, last_params().arguments.size());
}

TEST_F(DeveloperModeBindingsSystemUnittest,
       RestrictDeveloperModeAPIsUserIsNotInDeveloperModeAndHasPermission) {
  // With kDeveloperModeRestriction enabled, developer mode-only APIs
  // should not be available if the user is not in dev mode.
  SetCurrentDeveloperMode(kRendererProfileId, false);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermission("debugger").Build();
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
    v8::Local<v8::Function> call_api_method =
        FunctionFromString(context, kCallDebuggerGetTargets);
    std::string expected_error =
        "Uncaught Error: The 'debugger' API is only available for users in "
        "developer mode.";
    RunFunctionAndExpectError(call_api_method, context, 0, nullptr,
                              expected_error);
  }
}

TEST_F(
    DeveloperModeBindingsSystemUnittest,
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

  v8::Local<v8::Value> api = GetPropertyFromObject(
      v8::Local<v8::Object>::Cast(chrome), context, "debugger");
  ASSERT_FALSE(api.IsEmpty());
  EXPECT_TRUE(api->IsUndefined());
}

}  // namespace extensions

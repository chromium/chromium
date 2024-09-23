// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/values_test_util.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/storage_area.h"

namespace extensions {

class CustomTypesTest : public NativeExtensionBindingsSystemUnittest {
 public:
  CustomTypesTest()
      : extension_id_(crx_file::id_util::GenerateId("id")),
        allowlisted_extension_id_(extension_id_) {}

  CustomTypesTest(const CustomTypesTest&) = delete;
  CustomTypesTest& operator=(const CustomTypesTest&) = delete;

  ~CustomTypesTest() override = default;

  // Checks behavior of script after the main context is invalidated.
  // Creates an extension with the given |permission|, and then runs
  // |use_api_script| as a function with a single argument, the result of
  // |api_script|. This expects the function to succeed while the function is
  // valid, and then fail when the function is invalidated with the expected
  // error.
  // Note that no other validations are made (e.g., around the correctness of
  // the call made to the API).
  void RunContextInvalidationTest(const char* permission,
                                  const char* api_script,
                                  const char* use_api_script) {
    scoped_refptr<const Extension> extension = ExtensionBuilder("foo")
                                                   .AddAPIPermission(permission)
                                                   .SetID(extension_id_)
                                                   .Build();
    RegisterExtension(extension);

    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = MainContext();

    ScriptContext* script_context = CreateScriptContext(
        context, extension.get(), mojom::ContextType::kPrivilegedExtension);
    script_context->set_url(extension->url());

    bindings_system()->UpdateBindingsForContext(script_context);

    v8::Local<v8::Value> api_object =
        V8ValueFromScriptSource(context, api_script);
    ASSERT_TRUE(api_object->IsObject());

    v8::Local<v8::Function> use_api =
        FunctionFromString(context, use_api_script);
    v8::Local<v8::Value> args[] = {api_object};
    RunFunction(use_api, context, std::size(args), args);

    DisposeContext(context);

    EXPECT_FALSE(binding::IsContextValid(context));
    RunFunctionAndExpectError(use_api, context, std::size(args), args,
                              "Uncaught Error: Extension context invalidated.");
  }

 private:
  extensions::ExtensionId extension_id_;
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlisted_extension_id_;
};

TEST_F(CustomTypesTest, ContentSettingsUseAfterInvalidation) {
  RunContextInvalidationTest("contentSettings",
                             "chrome.contentSettings.javascript",
                             R"((function(setting) {
                                setting.set({
                                  primaryPattern: '<all_urls>',
                                  setting: 'block' });
                                });)");
}

TEST_F(CustomTypesTest, ChromeSettingsAPIUseAfterInvalidation) {
  RunContextInvalidationTest(
      "privacy", "chrome.privacy.websites.doNotTrackEnabled",
      R"((function(setting) { setting.set({value: true}); }))");
}

TEST_F(CustomTypesTest, ChromeSettingsEventUseAfterInvalidation) {
  RunContextInvalidationTest("privacy",
                             "chrome.privacy.websites.doNotTrackEnabled",
                             R"((function(setting) {
                                  setting.onChange.addListener(function() {});
                                });)");
}

TEST_F(CustomTypesTest, ContentSettingsPromisesForManifestV3) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .SetManifestVersion(3)
          .AddAPIPermission("contentSettings")
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> settings =
      V8ValueFromScriptSource(context, "chrome.contentSettings");
  ASSERT_TRUE(settings->IsObject());

  // Invoking ContentSetting.get() without the required callback should work
  // and use the promise-based version of the API.
  {
    constexpr char kRunGetContentSetting[] = R"(
        (function(settings) {
          return settings.javascript.get({primaryUrl: "https://chrome.org"});
        }))";
    v8::Local<v8::Function> run_get_content_setting =
        FunctionFromString(context, kRunGetContentSetting);
    v8::Local<v8::Value> args[] = {settings};
    v8::Local<v8::Value> return_value = RunFunctionOnGlobal(
        run_get_content_setting, context, std::size(args), args);

    ASSERT_TRUE(return_value->IsPromise());
    v8::Local<v8::Promise> promise = return_value.As<v8::Promise>();

    EXPECT_EQ(v8::Promise::kPending, promise->State());

    EXPECT_EQ(extension->id(), last_params().extension_id);
    EXPECT_EQ("contentSettings.get", last_params().name);
    EXPECT_EQ(extension->url(), last_params().source_url);
    // We treat returning a promise as having a callback in the request params.
    EXPECT_TRUE(last_params().has_callback);
    EXPECT_THAT(last_params().arguments,
                base::test::IsJson(
                    R"(["javascript", {"primaryUrl": "https://chrome.org"}])"));

    bindings_system()->HandleResponse(
        last_params().request_id, /*success=*/true,
        ListValueFromString(R"([{"setting": "block"}])"),
        /*error=*/std::string());

    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    EXPECT_EQ(R"({"setting":"block"})", V8ToString(promise->Result(), context));
  }

  // Invoking ContentSetting.set() without the required argument should trigger
  // an error.
  {
    constexpr char kRunSetContentSetting[] =
        "(function(settings) { settings.javascript.set(); })";
    v8::Local<v8::Function> run_set_content_setting =
        FunctionFromString(context, kRunSetContentSetting);
    v8::Local<v8::Value> args[] = {settings};
    RunFunctionAndExpectError(
        run_set_content_setting, context, std::size(args), args,
        "Uncaught TypeError: " +
            api_errors::InvocationError(
                "contentSettings.ContentSetting.set",
                "object details, optional function callback",
                "No matching signature."));
  }
}

TEST_F(CustomTypesTest, ContentSettingsInvalidInvocationForManifestV2) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .SetManifestVersion(2)
          .AddAPIPermission("contentSettings")
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> settings =
      V8ValueFromScriptSource(context, "chrome.contentSettings");
  ASSERT_TRUE(settings->IsObject());

  // Invoking ContentSetting.get() without the required callback should
  // trigger an error and not get the promise-based version of the API.
  {
    constexpr char kRunGetContentSetting[] = R"(
        (function(settings) {
          return settings.javascript.get({primaryUrl: "https://chrome.org"});
        }))";
    v8::Local<v8::Function> run_get_content_setting =
        FunctionFromString(context, kRunGetContentSetting);
    v8::Local<v8::Value> args[] = {settings};
    RunFunctionAndExpectError(
        run_get_content_setting, context, std::size(args), args,
        "Uncaught TypeError: " +
            api_errors::InvocationError("contentSettings.ContentSetting.get",
                                        "object details, function callback",
                                        "No matching signature."));
  }

  // Invoking ContentSetting.set() without the required argument should trigger
  // an error.
  {
    constexpr char kRunSetContentSetting[] =
        "(function(settings) { settings.javascript.set(); })";
    v8::Local<v8::Function> run_set_content_setting =
        FunctionFromString(context, kRunSetContentSetting);
    v8::Local<v8::Value> args[] = {settings};
    RunFunctionAndExpectError(
        run_set_content_setting, context, std::size(args), args,
        "Uncaught TypeError: " +
            api_errors::InvocationError(
                "contentSettings.ContentSetting.set",
                "object details, optional function callback",
                "No matching signature."));
  }
}

TEST_F(CustomTypesTest, ChromeSettingPromisesForManifestV3) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("foo")
                                                 .SetManifestVersion(3)
                                                 .AddAPIPermission("privacy")
                                                 .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> settings =
      V8ValueFromScriptSource(context, "chrome.privacy");
  ASSERT_TRUE(settings->IsObject());

  // Invoking ChromeSetting.get() without the required callback should work
  // and use the promise-based version of the API.
  {
    constexpr char kRunGetChromeSetting[] = R"(
        (function(settings) {
          return settings.websites.doNotTrackEnabled.get({});
        }))";
    v8::Local<v8::Function> run_get_chrome_setting =
        FunctionFromString(context, kRunGetChromeSetting);
    v8::Local<v8::Value> args[] = {settings};
    v8::Local<v8::Value> return_value = RunFunctionOnGlobal(
        run_get_chrome_setting, context, std::size(args), args);

    ASSERT_TRUE(return_value->IsPromise());
    v8::Local<v8::Promise> promise = return_value.As<v8::Promise>();

    EXPECT_EQ(v8::Promise::kPending, promise->State());

    EXPECT_EQ(extension->id(), last_params().extension_id);
    EXPECT_EQ("types.ChromeSetting.get", last_params().name);
    EXPECT_EQ(extension->url(), last_params().source_url);
    // We treat returning a promise as having a callback in the request params.
    EXPECT_TRUE(last_params().has_callback);
    EXPECT_THAT(last_params().arguments,
                base::test::IsJson(R"(["doNotTrackEnabled", {}])"));

    bindings_system()->HandleResponse(
        last_params().request_id, /*success=*/true,
        ListValueFromString(R"([{"value": false}])"),
        /*error=*/std::string());

    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    EXPECT_EQ(R"({"value":false})", V8ToString(promise->Result(), context));
  }

  // Invoking ChromeSetting.set() without the required argument should trigger
  // an error.
  {
    constexpr char kRunSetChromeSetting[] =
        "(function(settings) { settings.websites.doNotTrackEnabled.set(); })";
    v8::Local<v8::Function> run_set_chrome_setting =
        FunctionFromString(context, kRunSetChromeSetting);
    v8::Local<v8::Value> args[] = {settings};
    RunFunctionAndExpectError(
        run_set_chrome_setting, context, std::size(args), args,
        "Uncaught TypeError: " +
            api_errors::InvocationError(
                "types.ChromeSetting.set",
                "object details, optional function callback",
                "No matching signature."));
  }
}

TEST_F(CustomTypesTest, ChromeSettingInvalidInvocationForManifestV2) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("foo")
                                                 .SetManifestVersion(2)
                                                 .AddAPIPermission("privacy")
                                                 .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> settings =
      V8ValueFromScriptSource(context, "chrome.privacy");
  ASSERT_TRUE(settings->IsObject());

  // Invoking ChromeSetting.get() without the required callback should trigger
  // an error and not get the promise-based version of the API.
  {
    constexpr char kRunGetChromeSetting[] =
        "(function(settings) { return "
        "settings.websites.doNotTrackEnabled.get({}); })";
    v8::Local<v8::Function> run_get_chrome_setting =
        FunctionFromString(context, kRunGetChromeSetting);
    v8::Local<v8::Value> args[] = {settings};
    RunFunctionAndExpectError(
        run_get_chrome_setting, context, std::size(args), args,
        "Uncaught TypeError: " +
            api_errors::InvocationError("types.ChromeSetting.get",
                                        "object details, function callback",
                                        "No matching signature."));
  }

  // Invoking ChromeSetting.set() without the required argument to trigger an
  // error.
  {
    constexpr char kRunSetChromeSetting[] =
        "(function(settings) { settings.websites.doNotTrackEnabled.set(); })";
    v8::Local<v8::Function> run_set_chrome_setting =
        FunctionFromString(context, kRunSetChromeSetting);
    v8::Local<v8::Value> args[] = {settings};
    RunFunctionAndExpectError(
        run_set_chrome_setting, context, std::size(args), args,
        "Uncaught TypeError: " +
            api_errors::InvocationError(
                "types.ChromeSetting.set",
                "object details, optional function callback",
                "No matching signature."));
  }
}

}  // namespace extensions

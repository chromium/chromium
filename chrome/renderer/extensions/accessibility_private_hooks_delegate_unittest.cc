// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/accessibility_private_hooks_delegate.h"

#include "base/strings/stringprintf.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

using AccessibilityPrivateHooksDelegateTest =
    NativeExtensionBindingsSystemUnittest;

TEST_F(AccessibilityPrivateHooksDelegateTest, TestGetDisplayLanguage) {
  // Initialize bindings system.
  bindings_system()
      ->api_system()
      ->GetHooksForAPI("accessibilityPrivate")
      ->SetDelegate(std::make_unique<AccessibilityPrivateHooksDelegate>());
  // Register extension.
  // Ensure that the extension has access to the accessibilityPrivate API by
  // setting the extension ID to the ChromeVox extension ID, as well as giving
  // it permission to the accessibilityPrivate API.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("testExtension")
          .AddPermission("accessibilityPrivate")
          .SetLocation(Manifest::COMPONENT)
          .Build();
  RegisterExtension(extension);
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(script_context);

  // Ensure that the extension has access to the accessibilityPrivate API.
  const Feature* a11y_private =
      FeatureProvider::GetAPIFeature("accessibilityPrivate");
  ASSERT_TRUE(a11y_private);
  Feature::Availability availability =
      a11y_private->IsAvailableToExtension(extension.get());
  ASSERT_TRUE(availability.is_available()) << availability.message();

  // Setup function that will run extension api.
  auto run_get_display_language = [context](const char* args) {
    SCOPED_TRACE(args);
    constexpr char kRunGetDisplayLanguageFunction[] =
        R"((function() {
          return chrome.accessibilityPrivate.getDisplayLanguage(%s);
        }))";
    v8::Local<v8::Function> function = FunctionFromString(
        context, base::StringPrintf(kRunGetDisplayLanguageFunction, args));
    v8::Local<v8::Value> result = RunFunction(function, context, 0, nullptr);
    return V8ToString(result, context);
  };

  // Test behavior.
  EXPECT_EQ(R"("")", run_get_display_language("'',''"));
  EXPECT_EQ(R"("")", run_get_display_language("'not a language code','ja-JP'"));
  EXPECT_EQ(R"("")",
            run_get_display_language("'zh-TW', 'not a language code'"));
  EXPECT_EQ(R"("English")", run_get_display_language("'en','en'"));
  EXPECT_EQ(R"("English")", run_get_display_language("'en-US','en'"));
  EXPECT_EQ(R"("français")", run_get_display_language("'fr','fr'"));
  EXPECT_EQ(R"("español")", run_get_display_language("'es','es'"));
  EXPECT_EQ(R"("日本語")", run_get_display_language("'ja','ja'"));
  EXPECT_EQ(R"("anglais")", run_get_display_language("'en','fr'"));
  EXPECT_EQ(R"("Japanese")", run_get_display_language("'ja','en'"));
}

}  // namespace extensions

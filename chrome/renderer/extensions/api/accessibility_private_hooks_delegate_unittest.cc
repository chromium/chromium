// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/accessibility_private_hooks_delegate.h"

#include "base/strings/stringprintf.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

using AccessibilityPrivateHooksDelegateTest =
    NativeExtensionBindingsSystemUnittest;

TEST_F(AccessibilityPrivateHooksDelegateTest, TestGetDisplayNameForLocale) {
  // Initialize bindings system.
  bindings_system()->api_system()->RegisterHooksDelegate(
      "accessibilityPrivate",
      std::make_unique<AccessibilityPrivateHooksDelegate>());
  // Register extension.
  // Ensure that the extension has access to the accessibilityPrivate API by
  // setting the extension ID to the ChromeVox extension ID, as well as giving
  // it permission to the accessibilityPrivate API.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("testExtension")
          .AddAPIPermission("accessibilityPrivate")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();
  RegisterExtension(extension);
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
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
  auto run_get_display_locale = [context](const char* args) {
    SCOPED_TRACE(args);
    constexpr char kRunGetDisplayLocale[] =
        R"((function() {
          return chrome.accessibilityPrivate.getDisplayNameForLocale(%s);
        }))";
    v8::Local<v8::Function> function = FunctionFromString(
        context, base::StringPrintf(kRunGetDisplayLocale, args));
    v8::Local<v8::Value> result = RunFunction(function, context, 0, nullptr);
    return V8ToString(result, context);
  };

  // Test behavior.
  EXPECT_EQ(R"("")", run_get_display_locale("'', ''"));
  EXPECT_EQ(R"("")", run_get_display_locale("'not a locale code', 'ja-JP'"));
  EXPECT_EQ(R"|("Chinese (Traditional)")|",
            run_get_display_locale("'zh-TW', 'en'"));
  EXPECT_EQ(R"("")", run_get_display_locale("'zh-TW', 'not a locale code'"));
  EXPECT_EQ(R"("English")", run_get_display_locale("'en', 'en'"));
  EXPECT_EQ(R"|("English (United States)")|",
            run_get_display_locale("'en-US', 'en'"));
  EXPECT_EQ(R"("français")", run_get_display_locale("'fr', 'fr-FR'"));
  EXPECT_EQ(R"|("français (France)")|",
            run_get_display_locale("'fr-FR', 'fr-FR'"));
  EXPECT_EQ(R"|("francés (Francia)")|",
            run_get_display_locale("'fr-FR', 'es'"));
  EXPECT_EQ(R"("日本語")", run_get_display_locale("'ja', 'ja'"));
  EXPECT_EQ(R"("anglais")", run_get_display_locale("'en', 'fr-CA'"));
  EXPECT_EQ(R"("Japanese")", run_get_display_locale("'ja', 'en'"));
}

}  // namespace extensions

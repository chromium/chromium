// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_fetcher.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_storage_service_impl.h"
#include "chrome/common/chrome_version.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using BrowserCommand = browser_command::mojom::Command;

// Enabled through feature list.
BASE_FEATURE(kTestModuleEnabled,
             "TestModuleEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enabled through feature list.
BASE_FEATURE(kTestModule2Enabled,
             "TestModule2Enabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Disabled through feature list.
BASE_FEATURE(kTestModuleDisabled,
             "TestModuleDisabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enabled by default.
BASE_FEATURE(kTestModuleEnabledByDefault,
             "TestModuleEnabledByDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Disabled by default.
BASE_FEATURE(kTestModuleDisabledByDefault,
             "TestModuleDisabledByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

class GlobalFeaturesFake : public GlobalFeatures {
 public:
  GlobalFeaturesFake() = default;

 protected:
  std::unique_ptr<whats_new::WhatsNewRegistry> CreateWhatsNewRegistry()
      override {
    auto registry = std::make_unique<whats_new::WhatsNewRegistry>(
        std::make_unique<whats_new::WhatsNewStorageServiceImpl>());

    return registry;
  }
};

std::unique_ptr<GlobalFeatures> CreateGlobalFeatures() {
  return std::make_unique<GlobalFeaturesFake>();
}

class WhatsNewFetcherBrowserTest : public InteractiveBrowserTest {
 public:
  WhatsNewFetcherBrowserTest() {
    GlobalFeatures::ReplaceGlobalFeaturesForTesting(
        base::BindRepeating(&CreateGlobalFeatures));
    feature_list_.InitWithFeatures({user_education::features::kWhatsNewVersion2,
                                    kTestModuleEnabled, kTestModule2Enabled},
                                   {kTestModuleDisabled});
  }
  ~WhatsNewFetcherBrowserTest() override {
    GlobalFeatures::ReplaceGlobalFeaturesForTesting(base::NullCallback());
  }

  whats_new::WhatsNewRegistry* GetRegistry() {
    return g_browser_process->GetFeatures()->whats_new_registry();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WhatsNewFetcherBrowserTest, GetV2ServerURL) {
  const std::string expected = base::StringPrintf(
      "https://www.google.com/chrome/v2/whats-new/?version=%d",
      CHROME_VERSION_MAJOR);

  EXPECT_EQ(expected, whats_new::GetV2ServerURL().possibly_invalid_spec());
}

IN_PROC_BROWSER_TEST_F(WhatsNewFetcherBrowserTest,
                       GetV2ServerURLForRenderNoFeatures) {
  std::string expected = base::StringPrintf(
      "https://www.google.com/chrome/v2/whats-new/?version=%d&internal=true",
      CHROME_VERSION_MAJOR);

  EXPECT_EQ(expected,
            whats_new::GetV2ServerURLForRender().possibly_invalid_spec());
}

IN_PROC_BROWSER_TEST_F(WhatsNewFetcherBrowserTest,
                       GetV2ServerStagingURLForRenderNoFeatures) {
  std::string expected = base::StringPrintf(
      "https://chrome-staging.corp.google.com/chrome/v2/whats-new/"
      "?version=%d&internal=true",
      CHROME_VERSION_MAJOR);

  EXPECT_EQ(expected,
            whats_new::GetV2ServerURLForRender(true).possibly_invalid_spec());
}

IN_PROC_BROWSER_TEST_F(WhatsNewFetcherBrowserTest,
                       GetV2ServerURLForRenderWithOneEnabled) {
  whats_new::WhatsNewRegistry* registry = GetRegistry();
  registry->RegisterModule(whats_new::WhatsNewModule(kTestModuleEnabled, ""));
  registry->RegisterModule(
      whats_new::WhatsNewModule("", "", BrowserCommand::kNoOpCommand));

  std::string expected = base::StringPrintf(
      "https://www.google.com/chrome/v2/whats-new/?version=%d",
      CHROME_VERSION_MAJOR);

  // Enabled modules will be sent with `enabled` parameter.
  expected.append(base::StringPrintf("&enabled=%s", kTestModuleEnabled.name));

  expected.append("&internal=true");

  EXPECT_EQ(expected,
            whats_new::GetV2ServerURLForRender().possibly_invalid_spec());
}

IN_PROC_BROWSER_TEST_F(WhatsNewFetcherBrowserTest,
                       GetV2ServerURLForRenderWithMultipleEnabled) {
  whats_new::WhatsNewRegistry* registry = GetRegistry();
  registry->RegisterModule(whats_new::WhatsNewModule(kTestModuleEnabled, ""));
  registry->RegisterModule(whats_new::WhatsNewModule(kTestModule2Enabled, ""));
  registry->RegisterModule(
      whats_new::WhatsNewModule("", "", BrowserCommand::kNoOpCommand));

  std::string expected = base::StringPrintf(
      "https://www.google.com/chrome/v2/whats-new/?version=%d",
      CHROME_VERSION_MAJOR);

  // Multiple enabled features will be comma-separated (url-encoded).
  expected.append(base::StringPrintf(
      "&enabled=%s%%2C%s", kTestModuleEnabled.name, kTestModule2Enabled.name));

  expected.append("&internal=true");

  EXPECT_EQ(expected,
            whats_new::GetV2ServerURLForRender().possibly_invalid_spec());
}

IN_PROC_BROWSER_TEST_F(WhatsNewFetcherBrowserTest,
                       GetV2ServerURLForRenderEnabledAndRolled) {
  whats_new::WhatsNewRegistry* registry = GetRegistry();
  registry->RegisterModule(whats_new::WhatsNewModule(kTestModuleEnabled, ""));
  // Will be ignored - disabled by experiment
  registry->RegisterModule(whats_new::WhatsNewModule(kTestModuleDisabled, ""));
  registry->RegisterModule(
      whats_new::WhatsNewModule(kTestModuleEnabledByDefault, ""));
  // Will be ignored - disabled by default
  registry->RegisterModule(
      whats_new::WhatsNewModule(kTestModuleDisabledByDefault, ""));
  // Will be ignored - no feature.
  registry->RegisterModule(
      whats_new::WhatsNewModule("", "", BrowserCommand::kNoOpCommand));

  std::string expected = base::StringPrintf(
      "https://www.google.com/chrome/v2/whats-new/?version=%d",
      CHROME_VERSION_MAJOR);

  // Enabled modules will be sent with `enabled` parameter.
  expected.append(base::StringPrintf("&enabled=%s", kTestModuleEnabled.name));

  // Enabled by default modules will be sent with `rolled` parameter.
  expected.append(
      base::StringPrintf("&rolled=%s", kTestModuleEnabledByDefault.name));

  expected.append("&internal=true");

  EXPECT_EQ(expected,
            whats_new::GetV2ServerURLForRender().possibly_invalid_spec());
}

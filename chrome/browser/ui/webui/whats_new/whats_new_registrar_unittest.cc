// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_registrar.h"

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/action_variants_reader.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "base/threading/thread_restrictions.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/user_education/webui/mock_whats_new_storage_service.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using BrowserCommand = browser_command::mojom::Command;

// Modules
BASE_FEATURE(kTestModule, "TestModule", base::FEATURE_DISABLED_BY_DEFAULT);

/// Editions
BASE_FEATURE(kTestEdition, "TestEdition", base::FEATURE_DISABLED_BY_DEFAULT);

void RegisterWhatsNewModulesForTests(whats_new::WhatsNewRegistry* registry) {
  // Test Module
  registry->RegisterModule(
      whats_new::WhatsNewModule(kTestModule, "mickeyburks@chromium.org"));
  registry->RegisterModule(
      whats_new::WhatsNewModule("TestDefaultModule", "mickeyburks@chromium.org",
                                BrowserCommand::kNoOpCommand));
}

void RegisterWhatsNewEditionsForTests(whats_new::WhatsNewRegistry* registry) {
  // Test Edition
  registry->RegisterEdition(
      whats_new::WhatsNewEdition(kTestEdition, "mickeyburks@chromium.org"));
}

}  // namespace

TEST(WhatsNewRegistrarTest, CheckModuleHistograms) {
  std::optional<base::HistogramVariantsEntryMap> variants;
  std::vector<std::string> missing_modules;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    variants =
        base::ReadVariantsFromHistogramsXml("WhatsNewModule", "user_education");
    ASSERT_TRUE(variants.has_value());
  }

  whats_new::WhatsNewRegistry registry(
      std::make_unique<MockWhatsNewStorageService>());
  RegisterWhatsNewModules(&registry);
  RegisterWhatsNewModulesForTests(&registry);
  const auto& modules = registry.modules();
  for (const auto& [key, module] : modules) {
    const auto metric_name = module.unique_name();
    if (!base::Contains(*variants, metric_name)) {
      missing_modules.emplace_back(metric_name);
    }
  }
  ASSERT_TRUE(missing_modules.empty())
      << "What's New Modules:\n"
      << base::JoinString(missing_modules, ", ")
      << "\nconfigured in whats_new_registrar.cc but no "
         "corresponding variants were added to WhatsNewModule variants in "
         "//tools/metrics/histograms/metadata/user_education/"
         "histograms.xml";
}

TEST(WhatsNewRegistrarTest, CheckModuleActions) {
  std::vector<base::test::ActionVariantsEntryMap> variants;
  std::vector<std::string> missing_modules;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    variants = base::test::ReadActionVariantsForAction(
        "UserEducation.WhatsNew.ModuleShown", ".");
    ASSERT_EQ(1U, variants.size());
  }

  whats_new::WhatsNewRegistry registry(
      std::make_unique<MockWhatsNewStorageService>());
  RegisterWhatsNewModules(&registry);
  RegisterWhatsNewModulesForTests(&registry);
  const auto& modules = registry.modules();
  for (const auto& [key, module] : modules) {
    const auto metric_name = module.unique_name();
    if (!base::Contains(variants[0], metric_name)) {
      missing_modules.emplace_back(metric_name);
    }
  }
  ASSERT_TRUE(missing_modules.empty())
      << "Whats's New Modules:\n"
      << base::JoinString(missing_modules, ", ")
      << "\nconfigured in whats_new_registrar.cc but no "
         "corresponding action variants were added in "
         "//tools/metrics/actions/actions.xml";
}

TEST(WhatsNewRegistrarTest, CheckEditionActions) {
  std::vector<base::test::ActionVariantsEntryMap> variants;
  std::vector<std::string> missing_editions;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    variants = base::test::ReadActionVariantsForAction(
        "UserEducation.WhatsNew.EditionShown", ".");
    ASSERT_EQ(1U, variants.size());
  }

  whats_new::WhatsNewRegistry registry(
      std::make_unique<MockWhatsNewStorageService>());
  RegisterWhatsNewEditions(&registry);
  RegisterWhatsNewEditionsForTests(&registry);
  for (const auto& [key, edition] : registry.editions()) {
    const auto metric_name = edition.unique_name();
    if (!base::Contains(variants[0], metric_name)) {
      missing_editions.emplace_back(metric_name);
    }
  }
  ASSERT_TRUE(missing_editions.empty())
      << "Whats's New Editions:\n"
      << base::JoinString(missing_editions, ", ")
      << "\nconfigured in whats_new_registrar.cc but no "
         "corresponding action variants were added in "
         "//tools/metrics/actions/actions.xml";
}

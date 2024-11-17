// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_registrar.h"

#include "base/containers/contains.h"
#include "base/test/metrics/action_suffix_reader.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "base/threading/thread_restrictions.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "components/user_education/webui/whats_new_storage_service.h"
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

class MockWhatsNewStorageService : public whats_new::WhatsNewStorageService {
 public:
  MockWhatsNewStorageService() = default;
  MOCK_METHOD(const base::Value::List&, ReadModuleData, (), (const override));
  MOCK_METHOD(const base::Value::Dict&, ReadEditionData, (), (const, override));
  MOCK_METHOD(int,
              GetModuleQueuePosition,
              (const std::string_view),
              (const, override));
  MOCK_METHOD(std::optional<int>,
              GetUsedVersion,
              (std::string_view edition_name),
              (const override));
  MOCK_METHOD(std::optional<std::string_view>,
              FindEditionForCurrentVersion,
              (),
              (const, override));
  MOCK_METHOD(bool, IsUsedEdition, (const std::string_view), (const, override));
  MOCK_METHOD(void, SetModuleEnabled, (const std::string_view), (override));
  MOCK_METHOD(void, ClearModule, (const std::string_view), (override));
  MOCK_METHOD(void, SetEditionUsed, (const std::string_view), (override));
  MOCK_METHOD(void, ClearEdition, (const std::string_view), (override));
  MOCK_METHOD(void, Reset, (), (override));
};

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
  for (const auto& module : modules) {
    const auto metric_name = module.metric_name();
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
  std::vector<base::ActionSuffixEntryMap> suffixes;
  std::vector<std::string> missing_modules;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    suffixes =
        base::ReadActionSuffixesForAction("UserEducation.WhatsNew.ModuleShown");
    ASSERT_EQ(1U, suffixes.size());
  }

  whats_new::WhatsNewRegistry registry(
      std::make_unique<MockWhatsNewStorageService>());
  RegisterWhatsNewModules(&registry);
  RegisterWhatsNewModulesForTests(&registry);
  const auto& modules = registry.modules();
  for (const auto& module : modules) {
    const auto metric_name = module.metric_name();
    if (!base::Contains(suffixes[0], metric_name)) {
      missing_modules.emplace_back(metric_name);
    }
  }
  ASSERT_TRUE(missing_modules.empty())
      << "Whats's New Modules:\n"
      << base::JoinString(missing_modules, ", ")
      << "\nconfigured in whats_new_registrar.cc but no "
         "corresponding action suffixes were added in "
         "//tools/metrics/actions/actions.xml";
}

TEST(WhatsNewRegistrarTest, CheckEditionActions) {
  std::vector<base::ActionSuffixEntryMap> suffixes;
  std::vector<std::string> missing_editions;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    suffixes = base::ReadActionSuffixesForAction(
        "UserEducation.WhatsNew.EditionShown");
    ASSERT_EQ(1U, suffixes.size());
  }

  whats_new::WhatsNewRegistry registry(
      std::make_unique<MockWhatsNewStorageService>());
  RegisterWhatsNewEditions(&registry);
  RegisterWhatsNewEditionsForTests(&registry);
  const auto& editions = registry.editions();
  for (const auto& edition : editions) {
    const auto metric_name = edition.metric_name();
    if (!base::Contains(suffixes[0], metric_name)) {
      missing_editions.emplace_back(metric_name);
    }
  }
  ASSERT_TRUE(missing_editions.empty())
      << "Whats's New Editions:\n"
      << base::JoinString(missing_editions, ", ")
      << "\nconfigured in whats_new_registrar.cc but no "
         "corresponding action suffixes were added in "
         "//tools/metrics/actions/actions.xml";
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/whats_new_registry.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_education/common/user_education_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"

using whats_new::WhatsNewModule;
using whats_new::WhatsNewRegistry;

namespace user_education {

namespace {

using BrowserCommand = browser_command::mojom::Command;

// Enabled through feature list.
BASE_FEATURE(kTestModuleEnabled,
             "TestModuleEnabled",
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

class WhatsNewRegistryTest : public testing::Test {
 public:
  WhatsNewRegistryTest() = default;
  ~WhatsNewRegistryTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    feature_list_.InitWithFeatures(
        {features::kWhatsNewVersion2, kTestModuleEnabled},
        {kTestModuleDisabled});

    whats_new_registry_ = std::make_unique<WhatsNewRegistry>();
    whats_new_registry_->RegisterModule(
        WhatsNewModule(&kTestModuleEnabled, "", BrowserCommand::kNoOpCommand));
    whats_new_registry_->RegisterModule(
        WhatsNewModule(&kTestModuleDisabled, "", BrowserCommand::kMinValue));
    whats_new_registry_->RegisterModule(
        WhatsNewModule(&kTestModuleEnabledByDefault, ""));
    whats_new_registry_->RegisterModule(
        WhatsNewModule(&kTestModuleDisabledByDefault, ""));
  }

  void TearDown() override {
    whats_new_registry_.reset();
    testing::Test::TearDown();
  }

 protected:
  std::unique_ptr<WhatsNewRegistry> whats_new_registry_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(WhatsNewRegistryTest, CommandsAreActiveForEnabledFeatures) {
  auto active_commands = whats_new_registry_->GetActiveCommands();
  EXPECT_EQ(static_cast<size_t>(1), active_commands.size());
  EXPECT_EQ(BrowserCommand::kNoOpCommand, active_commands.at(0));
}

TEST_F(WhatsNewRegistryTest, FindModulesForActiveFeatures) {
  auto active_features = whats_new_registry_->GetActiveFeatureNames();
  EXPECT_EQ(static_cast<size_t>(1), active_features.size());
  EXPECT_EQ("TestModuleEnabled", active_features.at(0));
}

TEST_F(WhatsNewRegistryTest, FindModulesForRolledFeatures) {
  auto rolled_features = whats_new_registry_->GetRolledFeatureNames();
  EXPECT_EQ(static_cast<size_t>(1), rolled_features.size());
  EXPECT_EQ("TestModuleEnabledByDefault", rolled_features.at(0));
}

}  // namespace user_education

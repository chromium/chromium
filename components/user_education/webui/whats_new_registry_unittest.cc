// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/whats_new_registry.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_education/common/user_education_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"

using testing::Return;
using whats_new::WhatsNewEdition;
using whats_new::WhatsNewModule;
using whats_new::WhatsNewRegistry;
using whats_new::WhatsNewStorageService;

namespace user_education {

namespace {

using BrowserCommand = browser_command::mojom::Command;

// Modules
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

// Editions
// Enabled through feature list.
BASE_FEATURE(kTestEditionEnabled1,
             "TestEditionEnabled1",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enabled through feature list.
BASE_FEATURE(kTestEditionEnabled2,
             "TestEditionEnabled2",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enabled by default.
BASE_FEATURE(kTestEditionEnabledByDefault,
             "TestEditionEnabledByDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Disabled by default.
BASE_FEATURE(kTestEditionDisabled,
             "TestEditionDisabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Previously used edition, enabled by default, unregistered.
BASE_FEATURE(kTestOldUnregisteredEdition,
             "TestOldUnregisteredEdition",
             base::FEATURE_ENABLED_BY_DEFAULT);

class MockWhatsNewStorageService : public whats_new::WhatsNewStorageService {
 public:
  MockWhatsNewStorageService() = default;
  MOCK_METHOD(const base::Value::List&, ReadModuleData, (), (const override));
  MOCK_METHOD(std::optional<int>,
              GetUsedVersion,
              (std::string_view edition_name),
              (const override));
  MOCK_METHOD(int,
              GetModuleQueuePosition,
              (std::string_view module_name),
              (const));
  MOCK_METHOD(const base::Value::Dict&, ReadEditionData, (), (const, override));
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

class WhatsNewRegistryTest : public testing::Test {
 public:
  WhatsNewRegistryTest() = default;
  ~WhatsNewRegistryTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    feature_list_.InitWithFeatures(
        {features::kWhatsNewVersion2, kTestModuleEnabled, kTestEditionEnabled1,
         kTestEditionEnabled2},
        {kTestModuleDisabled});
  }

  void CreateRegistry(
      std::unique_ptr<MockWhatsNewStorageService> mock_storage_service) {
    // One Enabled module
    EXPECT_CALL(*mock_storage_service, SetModuleEnabled(testing::_)).Times(2);

    // Create list of enabled modules in the same order as they are
    // registered below.
    stored_enabled_modules_.Append(kTestModuleEnabled.name);
    stored_enabled_modules_.Append(kTestModuleEnabledByDefault.name);

    // Create list of used editions.
    stored_used_editions_.Set(kTestOldUnregisteredEdition.name, 100);

    whats_new_registry_ =
        std::make_unique<WhatsNewRegistry>(std::move(mock_storage_service));
  }

  void RegisterModules(
      std::unique_ptr<MockWhatsNewStorageService> mock_storage_service) {
    CreateRegistry(std::move(mock_storage_service));

    // Modules
    whats_new_registry_->RegisterModule(
        WhatsNewModule(kTestModuleEnabled, "", BrowserCommand::kNoOpCommand));
    whats_new_registry_->RegisterModule(
        WhatsNewModule(kTestModuleDisabled, "", BrowserCommand::kMinValue));
    whats_new_registry_->RegisterModule(
        WhatsNewModule(kTestModuleEnabledByDefault, ""));
    whats_new_registry_->RegisterModule(
        WhatsNewModule(kTestModuleDisabledByDefault, ""));
    whats_new_registry_->RegisterModule(
        WhatsNewModule("", "", BrowserCommand::kUnknownCommand));
  }

  void RegisterModulesAndEditions(
      std::unique_ptr<MockWhatsNewStorageService> mock_storage_service) {
    RegisterModules(std::move(mock_storage_service));

    // Editions
    whats_new_registry_->RegisterEdition(WhatsNewEdition(
        kTestEditionEnabled1, "",
        {BrowserCommand::kOpenAISettings, BrowserCommand::kOpenSafetyCheck}));
    whats_new_registry_->RegisterEdition(
        WhatsNewEdition(kTestEditionEnabled2, ""));
    whats_new_registry_->RegisterEdition(
        WhatsNewEdition(kTestEditionEnabledByDefault, ""));
    whats_new_registry_->RegisterEdition(
        WhatsNewEdition(kTestEditionDisabled, ""));
  }

  void TearDown() override {
    stored_enabled_modules_.clear();
    stored_used_editions_.clear();
    whats_new_registry_.reset();
    testing::Test::TearDown();
  }

 protected:
  base::Value::List stored_enabled_modules_;
  base::Value::Dict stored_used_editions_;
  std::unique_ptr<WhatsNewRegistry> whats_new_registry_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(WhatsNewRegistryTest, CommandsAreActiveForEnabledFeatures) {
  auto mock_storage_service = std::make_unique<MockWhatsNewStorageService>();
  RegisterModules(std::move(mock_storage_service));

  auto active_commands = whats_new_registry_->GetActiveCommands();
  EXPECT_EQ(static_cast<size_t>(2), active_commands.size());
  EXPECT_EQ(BrowserCommand::kNoOpCommand, active_commands.at(0));
  EXPECT_EQ(BrowserCommand::kUnknownCommand, active_commands.at(1));
}

TEST_F(WhatsNewRegistryTest, CommandsAreActiveForEnabledModulesAndEditions) {
  auto mock_storage_service = std::make_unique<MockWhatsNewStorageService>();
  RegisterModulesAndEditions(std::move(mock_storage_service));

  auto active_commands = whats_new_registry_->GetActiveCommands();
  EXPECT_EQ(static_cast<size_t>(4), active_commands.size());
  EXPECT_EQ(BrowserCommand::kNoOpCommand, active_commands.at(0));
  EXPECT_EQ(BrowserCommand::kUnknownCommand, active_commands.at(1));

  // Note: If you are removing one of these commands, you may change
  // these to any available command to match the above Edition
  // registratrion.
  EXPECT_EQ(BrowserCommand::kOpenAISettings, active_commands.at(2));
  EXPECT_EQ(BrowserCommand::kOpenSafetyCheck, active_commands.at(3));
}

TEST_F(WhatsNewRegistryTest, FindModulesForActiveFeatures) {
  auto mock_storage_service = std::make_unique<MockWhatsNewStorageService>();
  EXPECT_CALL(*mock_storage_service, FindEditionForCurrentVersion())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*mock_storage_service, ReadModuleData())
      .WillOnce(testing::ReturnRef(stored_enabled_modules_));

  RegisterModules(std::move(mock_storage_service));

  auto active_features = whats_new_registry_->GetActiveFeatureNames();
  EXPECT_EQ(static_cast<size_t>(1), active_features.size());
  EXPECT_EQ("TestModuleEnabled", active_features.at(0));
}

TEST_F(WhatsNewRegistryTest, FindModulesForActiveFeaturesWithEditions) {
  auto mock_storage_service = std::make_unique<MockWhatsNewStorageService>();
  // No used edition.
  EXPECT_CALL(*mock_storage_service, FindEditionForCurrentVersion())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*mock_storage_service, ReadModuleData())
      .WillOnce(testing::ReturnRef(stored_enabled_modules_));
  EXPECT_CALL(*mock_storage_service, IsUsedEdition(testing::_))
      .WillRepeatedly(Return(false));
  RegisterModulesAndEditions(std::move(mock_storage_service));

  auto active_features = whats_new_registry_->GetActiveFeatureNames();
  EXPECT_EQ(static_cast<size_t>(4), active_features.size());
  // Editions appear first, in register order.
  EXPECT_EQ("TestEditionEnabled1", active_features.at(0));
  EXPECT_EQ("TestEditionEnabled2", active_features.at(1));
  EXPECT_EQ("TestEditionEnabledByDefault", active_features.at(2));
  // Modules appear last.
  EXPECT_EQ("TestModuleEnabled", active_features.at(3));
}

TEST_F(WhatsNewRegistryTest, FindModulesForActiveFeaturesWithUsedEdition) {
  auto mock_storage_service = std::make_unique<MockWhatsNewStorageService>();
  // Used for current version.
  EXPECT_CALL(*mock_storage_service, FindEditionForCurrentVersion())
      .WillOnce(Return(std::optional(kTestEditionEnabled1.name)));
  EXPECT_CALL(*mock_storage_service, ReadModuleData())
      .WillOnce(testing::ReturnRef(stored_enabled_modules_));
  RegisterModulesAndEditions(std::move(mock_storage_service));

  auto active_features = whats_new_registry_->GetActiveFeatureNames();
  EXPECT_EQ(static_cast<size_t>(2), active_features.size());
  // Used edition appears first.
  EXPECT_EQ("TestEditionEnabled1", active_features.at(0));
  // No other editions are added.
  // Modules appear last.
  EXPECT_EQ("TestModuleEnabled", active_features.at(1));
}

TEST_F(WhatsNewRegistryTest, FindModulesForRolledFeatures) {
  auto mock_storage_service = std::make_unique<MockWhatsNewStorageService>();
  EXPECT_CALL(*mock_storage_service, ReadModuleData())
      .WillOnce(testing::ReturnRef(stored_enabled_modules_));
  RegisterModules(std::move(mock_storage_service));

  auto rolled_features = whats_new_registry_->GetRolledFeatureNames();
  EXPECT_EQ(static_cast<size_t>(1), rolled_features.size());
  EXPECT_EQ("TestModuleEnabledByDefault", rolled_features.at(0));
}

TEST_F(WhatsNewRegistryTest, SetEditionUsed) {
  auto mock_storage_service = std::make_unique<MockWhatsNewStorageService>();
  EXPECT_CALL(*mock_storage_service, SetEditionUsed("TestEditionEnabled1"));
  RegisterModulesAndEditions(std::move(mock_storage_service));

  whats_new_registry_->SetEditionUsed(kTestEditionEnabled1.name);
}

TEST_F(WhatsNewRegistryTest, ClearUnregisteredModules) {
  auto mock_storage_service = std::make_unique<MockWhatsNewStorageService>();
  // Add unregistered module to stored data.
  stored_enabled_modules_.Append("TestModuleDoesNotExist");
  EXPECT_CALL(*mock_storage_service, ReadModuleData())
      .WillOnce(testing::ReturnRef(stored_enabled_modules_));
  EXPECT_CALL(*mock_storage_service, ClearModule("TestModuleDoesNotExist"));
  RegisterModules(std::move(mock_storage_service));

  whats_new_registry_->ClearUnregisteredModules();
}

TEST_F(WhatsNewRegistryTest, ClearUnregisteredEditions) {
  auto mock_storage_service = std::make_unique<MockWhatsNewStorageService>();
  EXPECT_CALL(*mock_storage_service, ReadEditionData())
      .WillOnce(testing::ReturnRef(stored_used_editions_));
  EXPECT_CALL(*mock_storage_service,
              ClearEdition("TestOldUnregisteredEdition"));
  RegisterModules(std::move(mock_storage_service));

  whats_new_registry_->ClearUnregisteredEditions();
}

TEST_F(WhatsNewRegistryTest, ResetStorageService) {
  auto mock_storage_service = std::make_unique<MockWhatsNewStorageService>();
  EXPECT_CALL(*mock_storage_service, Reset());
  RegisterModules(std::move(mock_storage_service));

  whats_new_registry_->ResetData();
}

}  // namespace user_education

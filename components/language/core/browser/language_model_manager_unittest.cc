// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_model_manager.h"

#include <memory>

#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/language_model/fluent_language_model.h"
#include "components/language/core/language_model/ulp_language_model.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {

struct PrefRegistration {
  explicit PrefRegistration(user_prefs::PrefRegistrySyncable* registry) {
    language::LanguagePrefs::RegisterProfilePrefs(registry);
    translate::TranslatePrefs::RegisterProfilePrefs(registry);
  }
};

class LanguageModelManagerTest : public testing::Test {
 protected:
  LanguageModelManagerTest()
      : prefs_registration_(prefs_.registry()), manager_(&prefs_, "en") {}

  void SetUp() override {
    manager_.AddModel(LanguageModelManager::ModelType::FLUENT,
                      std::make_unique<FluentLanguageModel>(&prefs_));
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  PrefRegistration prefs_registration_;
  LanguageModelManager manager_;
};

TEST_F(LanguageModelManagerTest, GetPrimaryModelTypeTest) {
  // Default primary model type is BASELINE.
  EXPECT_EQ(manager_.GetPrimaryModelType(),
            LanguageModelManager::ModelType::BASELINE);

  // Change primary model to FLUENT.
  manager_.SetPrimaryModel(LanguageModelManager::ModelType::FLUENT);
  EXPECT_EQ(manager_.GetPrimaryModelType(),
            LanguageModelManager::ModelType::FLUENT);
}

TEST_F(LanguageModelManagerTest, GetPrimaryModelTest) {
  // Default primary model type is BASELINE, but test manager only has the
  // fluent model set.
  EXPECT_EQ(manager_.GetPrimaryModel(), nullptr);

  // Change primary model to FLUENT, for which there is a corresponding
  // LanguageModel set.
  manager_.SetPrimaryModel(LanguageModelManager::ModelType::FLUENT);
  EXPECT_NE(manager_.GetPrimaryModel(), nullptr);
}

TEST_F(LanguageModelManagerTest, GetLanguageModelTest) {
  EXPECT_NE(manager_.GetLanguageModel(LanguageModelManager::ModelType::FLUENT),
            nullptr);

  // After adding a language model instance, the corresponding LanguageModel in
  // the test manager should be set.
  EXPECT_EQ(manager_.GetLanguageModel(LanguageModelManager::ModelType::ULP),
            nullptr);
  manager_.AddModel(LanguageModelManager::ModelType::ULP,
                    std::make_unique<ULPLanguageModel>());
  EXPECT_NE(manager_.GetLanguageModel(LanguageModelManager::ModelType::ULP),
            nullptr);
}
}  // namespace language

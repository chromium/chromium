// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_features.h"

#include "base/test/scoped_feature_list.h"
#include "components/skills/features.h"
#include "components/skills/public/skills_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

class SkillsFeaturesTest : public testing::Test {
 protected:
  void SetUp() override {
    prefs_ = std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    skills::prefs::RegisterProfilePrefs(prefs_->registry());
  }

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SkillsFeaturesTest, IsSkillsEnabled_FeatureOff_PrefOn) {
  feature_list_.InitAndDisableFeature(features::kSkillsEnabled);
  prefs_->SetBoolean(prefs::kChromeSkillsEnabled, true);

  EXPECT_FALSE(IsSkillsEnabled(prefs_.get()));
}

TEST_F(SkillsFeaturesTest, IsSkillsEnabled_FeatureOn_PrefOn) {
  feature_list_.InitAndEnableFeature(features::kSkillsEnabled);
  prefs_->SetBoolean(prefs::kChromeSkillsEnabled, true);

  EXPECT_TRUE(IsSkillsEnabled(prefs_.get()));
}

TEST_F(SkillsFeaturesTest, IsSkillsEnabled_FeatureOn_PrefOff) {
  feature_list_.InitAndEnableFeature(features::kSkillsEnabled);
  prefs_->SetBoolean(prefs::kChromeSkillsEnabled, false);

  EXPECT_FALSE(IsSkillsEnabled(prefs_.get()));
}

TEST_F(SkillsFeaturesTest, IsSkillsEnabled_FeatureOff_PrefOff) {
  feature_list_.InitAndDisableFeature(features::kSkillsEnabled);
  prefs_->SetBoolean(prefs::kChromeSkillsEnabled, false);

  EXPECT_FALSE(IsSkillsEnabled(prefs_.get()));
}

TEST_F(SkillsFeaturesTest, IsSkillsEnabled_NullPrefService) {
  feature_list_.InitAndEnableFeature(features::kSkillsEnabled);

  EXPECT_FALSE(IsSkillsEnabled(nullptr));
}

}  // namespace skills

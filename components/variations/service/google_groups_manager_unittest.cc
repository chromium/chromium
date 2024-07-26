// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/google_groups_manager.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/google_groups_manager_prefs.h"
#include "components/variations/variations_seed_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class GoogleGroupsManagerTest : public ::testing::Test {
 public:
  GoogleGroupsManagerTest() {
    key_ = "Default";
    target_prefs_.registry()->RegisterDictionaryPref(
        variations::prefs::kVariationsGoogleGroups);
    GoogleGroupsManager::RegisterProfilePrefs(source_prefs_.registry());
  }
  ~GoogleGroupsManagerTest() override = default;

  void SetSourcePref(std::vector<std::string> groups) {
    base::Value::List pref_groups_list;
    for (const std::string& group : groups) {
      base::Value::Dict group_dict;
      group_dict.Set(variations::kDogfoodGroupsSyncPrefGaiaIdKey, group);
      pref_groups_list.Append(std::move(group_dict));
    }
    source_prefs_.SetList(
#if BUILDFLAG(IS_CHROMEOS_ASH)
        variations::kOsDogfoodGroupsSyncPrefName,
#else
        variations::kDogfoodGroupsSyncPrefName,
#endif
        std::move(pref_groups_list));
  }

  void SetTargetPref(std::vector<std::string> groups) {
    base::Value::Dict groups_dict;
    base::Value::List pref_groups_list;
    for (const std::string& group : groups) {
      pref_groups_list.Append(group);
    }
    groups_dict.Set(key_, std::move(pref_groups_list));

    target_prefs_.SetDict(variations::prefs::kVariationsGoogleGroups,
                          std::move(groups_dict));
  }

  void CheckSourcePrefCleared() {
    EXPECT_TRUE(source_prefs_
                    .GetList(
#if BUILDFLAG(IS_CHROMEOS_ASH)
                        variations::kOsDogfoodGroupsSyncPrefName
#else
                        variations::kDogfoodGroupsSyncPrefName
#endif
                        )
                    .empty());
  }

  void CheckTargetPref(std::vector<std::string> expected_groups) {
    base::Value::List expected_list;
    for (const std::string& group : expected_groups) {
      expected_list.Append(group);
    }

    const base::Value::List* actual_list =
        target_prefs_.GetDict(variations::prefs::kVariationsGoogleGroups)
            .FindList(key_);

    EXPECT_EQ(*actual_list, expected_list);
  }

 protected:
  TestingPrefServiceSimple target_prefs_;
  std::string key_;
  sync_preferences::TestingPrefServiceSyncable source_prefs_;
};

TEST_F(GoogleGroupsManagerTest, NoSyncGroupsEmptyListWritten) {
  GoogleGroupsManager google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);

  // Check there are no groups in the target pref.
  CheckTargetPref({});
}

TEST_F(GoogleGroupsManagerTest, EmptySyncGroupsEmptyListWritten) {
  GoogleGroupsManager google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);
  SetSourcePref({});

  // Check there are no groups in the target pref.
  CheckTargetPref({});
}

TEST_F(GoogleGroupsManagerTest, SourceSyncGroupsWrittenToEmptyTarget) {
  GoogleGroupsManager google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);
  SetSourcePref({"123", "456"});

  // Check the groups have been copied to the target pref.
  CheckTargetPref({"123", "456"});
}

TEST_F(GoogleGroupsManagerTest,
       SourceSyncGroupsWrittenToNonEmptyTarget) {
  GoogleGroupsManager google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);
  SetTargetPref({"123", "456"});

  // Now update the source prefs (keeping one group, deleting one group, and
  // adding one group).
  SetSourcePref({"123", "789"});
  CheckTargetPref({"123", "789"});
}

TEST_F(GoogleGroupsManagerTest, ClearProfilePrefsNotPreviouslySet) {
  syncer::TestSyncService sync_service;
  GoogleGroupsManager google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);
  google_groups_updater.OnSyncServiceInitialized(&sync_service);
  // This just checks that ClearSigninScopedState() deals with the case where
  // the source pref was unset (i.e. is a no-op and doesn't crash).
  sync_service.SetSignedOut();
  google_groups_updater.OnStateChanged(&sync_service);

  CheckTargetPref({});
}

TEST_F(GoogleGroupsManagerTest, ClearProfilePrefsClearsTargetPref) {
  syncer::TestSyncService sync_service;
  GoogleGroupsManager google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);
  google_groups_updater.OnSyncServiceInitialized(&sync_service);
  SetSourcePref({"123", "456"});
  CheckTargetPref({"123", "456"});

  sync_service.SetSignedOut();
  google_groups_updater.OnStateChanged(&sync_service);

  // Check the source and target prefs have been cleared.
  CheckSourcePrefCleared();
  CheckTargetPref({});
}

// Tests that `IsFeatureEnabledForProfile` returns true if the feature is
// enabled and the source prefs of the `GoogleGroupsManager` contain at
// least one of the google_groups specified for the feature.
TEST_F(GoogleGroupsManagerTest, IsFeatureEnabledForProfile) {
  static BASE_FEATURE(kSampleFeature, "SampleFeature",
                      base::FEATURE_DISABLED_BY_DEFAULT);
  auto feature_list = std::make_unique<base::FeatureList>();
  GoogleGroupsManager google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);

  constexpr char kTrialName[] = "SampleTrial";
  constexpr char kGroupName[] = "SampleGroup";
  constexpr char kRelevantGroupId[] = "1234";
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  feature_list->RegisterFieldTrialOverride(
      "SampleFeature", base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  ASSERT_TRUE(base::AssociateFieldTrialParams(
      kTrialName, kGroupName,
      {{variations::internal::kGoogleGroupFeatureParamName,
        kRelevantGroupId}}));

  SetSourcePref({"123", "789"});
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kSampleFeature));
  EXPECT_FALSE(
      google_groups_updater.IsFeatureEnabledForProfile(kSampleFeature));

  SetSourcePref({"123", kRelevantGroupId});
  EXPECT_TRUE(base::FeatureList::IsEnabled(kSampleFeature));
  EXPECT_TRUE(google_groups_updater.IsFeatureEnabledForProfile(kSampleFeature));
}

// Tests that `IsFeatureEnabledForProfile` can deal properly with google_groups
// parameters of size longer than 1.
TEST_F(GoogleGroupsManagerTest,
       IsFeatureEnabledForProfileMultipleGroups) {
  static BASE_FEATURE(kSampleFeature, "SampleFeature",
                      base::FEATURE_DISABLED_BY_DEFAULT);
  auto feature_list = std::make_unique<base::FeatureList>();
  GoogleGroupsManager google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);

  constexpr char kTrialName[] = "SampleTrial";
  constexpr char kGroupName[] = "SampleGroup";
  constexpr char kRelevantGroupId[] = "1234";
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  feature_list->RegisterFieldTrialOverride(
      "SampleFeature", base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  ASSERT_TRUE(base::AssociateFieldTrialParams(
      kTrialName, kGroupName,
      {{variations::internal::kGoogleGroupFeatureParamName,
        base::StrCat({"645",
                      variations::internal::kGoogleGroupFeatureParamSeparator,
                      kRelevantGroupId})}}));

  SetSourcePref({kRelevantGroupId, "789"});
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kSampleFeature));
  EXPECT_TRUE(google_groups_updater.IsFeatureEnabledForProfile(kSampleFeature));
}

// Tests that `IsFeatureEnabledForProfile` is always false if the feature itself
// is disabled.
TEST_F(GoogleGroupsManagerTest,
       IsFeatureEnabledForProfileForDisabledFeature) {
  static BASE_FEATURE(kSampleFeature, "SampleFeature",
                      base::FEATURE_DISABLED_BY_DEFAULT);
  auto feature_list = std::make_unique<base::FeatureList>();
  GoogleGroupsManager google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);

  constexpr char kTrialName[] = "SampleTrial";
  constexpr char kGroupName[] = "SampleGroup";
  constexpr char kRelevantGroupId[] = "1234";
  base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  ASSERT_TRUE(base::AssociateFieldTrialParams(
      kTrialName, kGroupName,
      {{variations::internal::kGoogleGroupFeatureParamName,
        kRelevantGroupId}}));
  SetSourcePref({"123", kRelevantGroupId});
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kSampleFeature));
  EXPECT_FALSE(
      google_groups_updater.IsFeatureEnabledForProfile(kSampleFeature));
}

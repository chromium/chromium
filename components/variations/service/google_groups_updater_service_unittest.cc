// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/google_groups_updater_service.h"
#include "base/files/file_path.h"
#include "base/values.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/pref_names.h"
#include "google_groups_updater_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class GoogleGroupsUpdaterServiceTest : public ::testing::Test {
 public:
  GoogleGroupsUpdaterServiceTest() {
    key_ = "Default";
    target_prefs_.registry()->RegisterDictionaryPref(
        variations::prefs::kVariationsGoogleGroups);
    GoogleGroupsUpdaterService::RegisterProfilePrefs(source_prefs_.registry());
  }
  ~GoogleGroupsUpdaterServiceTest() override = default;

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

TEST_F(GoogleGroupsUpdaterServiceTest, NoSyncGroupsEmptyListWritten) {
  GoogleGroupsUpdaterService google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);

  // Check there are no groups in the target pref.
  CheckTargetPref({});
}

TEST_F(GoogleGroupsUpdaterServiceTest, EmptySyncGroupsEmptyListWritten) {
  GoogleGroupsUpdaterService google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);
  SetSourcePref({});

  // Check there are no groups in the target pref.
  CheckTargetPref({});
}

TEST_F(GoogleGroupsUpdaterServiceTest, SourceSyncGroupsWrittenToEmptyTarget) {
  GoogleGroupsUpdaterService google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);
  SetSourcePref({"123", "456"});

  // Check the groups have been copied to the target pref.
  CheckTargetPref({"123", "456"});
}

TEST_F(GoogleGroupsUpdaterServiceTest,
       SourceSyncGroupsWrittenToNonEmptyTarget) {
  GoogleGroupsUpdaterService google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);
  SetTargetPref({"123", "456"});

  // Now update the source prefs (keeping one group, deleting one group, and
  // adding one group).
  SetSourcePref({"123", "789"});
  CheckTargetPref({"123", "789"});
}

TEST_F(GoogleGroupsUpdaterServiceTest, ClearProfilePrefsNotPreviouslySet) {
  GoogleGroupsUpdaterService google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);
  // This just checks that ClearSigninScopedState() deals with the case where
  // the source pref was unset (i.e. is a no-op and doesn't crash).
  google_groups_updater.ClearSigninScopedState();

  CheckTargetPref({});
}

TEST_F(GoogleGroupsUpdaterServiceTest, ClearProfilePrefsClearsTargetPref) {
  GoogleGroupsUpdaterService google_groups_updater(target_prefs_, key_,
                                                   source_prefs_);
  SetSourcePref({"123", "456"});
  CheckTargetPref({"123", "456"});

  google_groups_updater.ClearSigninScopedState();

  // Check the source and target prefs have been cleared.
  CheckSourcePrefCleared();
  CheckTargetPref({});
}

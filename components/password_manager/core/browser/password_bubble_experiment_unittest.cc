// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_bubble_experiment.h"

#include <ostream>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_bubble_experiment {

namespace {

enum class CustomPassphraseState { NONE, SET };

}  // namespace

class PasswordManagerPasswordBubbleExperimentTest : public testing::Test {
 public:
  PasswordManagerPasswordBubbleExperimentTest() {
    signin::IdentityManager::RegisterProfilePrefs(pref_service_.registry());
  }

  syncer::TestSyncService* sync_service() { return &fake_sync_service_; }

 protected:
  void SetupFakeSyncServiceForTestCase(syncer::UserSelectableType type,
                                       CustomPassphraseState passphrase_state) {
    sync_service()->GetUserSettings()->SetSelectedTypes(false, {type});
    sync_service()->SetIsUsingExplicitPassphrase(passphrase_state ==
                                                 CustomPassphraseState::SET);
  }

 private:
  syncer::TestSyncService fake_sync_service_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordManagerPasswordBubbleExperimentTest, HasChosenToSyncPasswords) {
  constexpr struct {
    syncer::UserSelectableType type;
    CustomPassphraseState passphrase_state;
    bool expected_sync_user;
  } kTestData[] = {
      {syncer::UserSelectableType::kBookmarks, CustomPassphraseState::NONE,
       false},
      {syncer::UserSelectableType::kBookmarks, CustomPassphraseState::SET,
       false},
      {syncer::UserSelectableType::kPasswords, CustomPassphraseState::NONE,
       true},
      {syncer::UserSelectableType::kPasswords, CustomPassphraseState::SET,
       true},
  };
  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("#test_case = ") << (&test_case - kTestData));
    SetupFakeSyncServiceForTestCase(test_case.type, test_case.passphrase_state);

    EXPECT_EQ(test_case.expected_sync_user,
              HasChosenToSyncPasswords(sync_service()));
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       HasChosenToSyncPasswordsSyncFeatureDisabled) {
  SetupFakeSyncServiceForTestCase(syncer::UserSelectableType::kPasswords,
                                  CustomPassphraseState::NONE);
  sync_service()->SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY});

  EXPECT_FALSE(HasChosenToSyncPasswords(sync_service()));
}

}  // namespace password_bubble_experiment

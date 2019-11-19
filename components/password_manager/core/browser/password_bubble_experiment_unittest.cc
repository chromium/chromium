// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_bubble_experiment.h"

#include <ostream>

#include "base/strings/string_number_conversions.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_bubble_experiment {

namespace {

enum class CustomPassphraseState { NONE, SET };

}  // namespace

class PasswordManagerPasswordBubbleExperimentTest : public testing::Test {
 public:
  PasswordManagerPasswordBubbleExperimentTest() {
    RegisterPrefs(pref_service_.registry());
  }

  PrefService* prefs() { return &pref_service_; }

  syncer::TestSyncService* sync_service() { return &fake_sync_service_; }

 protected:
  void SetupFakeSyncServiceForTestCase(syncer::ModelType type,
                                       CustomPassphraseState passphrase_state) {
    sync_service()->SetPreferredDataTypes({type});
    sync_service()->SetActiveDataTypes({type});
    sync_service()->SetIsUsingSecondaryPassphrase(passphrase_state ==
                                                  CustomPassphraseState::SET);
  }

 private:
  syncer::TestSyncService fake_sync_service_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       ShouldShowChromeSignInPasswordPromo) {
  // By default the promo is off.
  EXPECT_FALSE(ShouldShowChromeSignInPasswordPromo(prefs(), nullptr));
  constexpr struct {
    bool was_already_clicked;
    bool is_sync_allowed;
    bool is_first_setup_complete;
    int current_shown_count;
    bool result;
  } kTestData[] = {
      {false, true, false, 0, true},   {false, true, false, 5, false},
      {true, true, false, 0, false},   {true, true, false, 10, false},
      {false, false, false, 0, false}, {false, true, true, 0, false},
  };
  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("#test_case = ") << (&test_case - kTestData));
    prefs()->SetBoolean(password_manager::prefs::kWasSignInPasswordPromoClicked,
                        test_case.was_already_clicked);
    prefs()->SetInteger(
        password_manager::prefs::kNumberSignInPasswordPromoShown,
        test_case.current_shown_count);
    sync_service()->SetDisableReasons(
        test_case.is_sync_allowed
            ? syncer::SyncService::DISABLE_REASON_NONE
            : syncer::SyncService::DISABLE_REASON_PLATFORM_OVERRIDE);
    sync_service()->SetFirstSetupComplete(test_case.is_first_setup_complete);
    sync_service()->SetTransportState(
        test_case.is_first_setup_complete
            ? syncer::SyncService::TransportState::ACTIVE
            : syncer::SyncService::TransportState::
                  PENDING_DESIRED_CONFIGURATION);

    EXPECT_EQ(test_case.result,
              ShouldShowChromeSignInPasswordPromo(prefs(), sync_service()));
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest, ReviveSignInPasswordPromo) {
  sync_service()->SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  sync_service()->SetFirstSetupComplete(false);
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION);
  prefs()->SetBoolean(password_manager::prefs::kWasSignInPasswordPromoClicked,
                      true);
  prefs()->SetInteger(password_manager::prefs::kNumberSignInPasswordPromoShown,
                      10);

  // The state is to be reset.
  EXPECT_TRUE(ShouldShowChromeSignInPasswordPromo(prefs(), sync_service()));
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest, IsSmartLockUser) {
  constexpr struct {
    syncer::ModelType type;
    CustomPassphraseState passphrase_state;
    bool expected_sync_user;
  } kTestData[] = {
      {syncer::ModelType::BOOKMARKS, CustomPassphraseState::NONE, false},
      {syncer::ModelType::BOOKMARKS, CustomPassphraseState::SET, false},
      {syncer::ModelType::PASSWORDS, CustomPassphraseState::NONE, true},
      {syncer::ModelType::PASSWORDS, CustomPassphraseState::SET, true},
  };
  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("#test_case = ") << (&test_case - kTestData));
    SetupFakeSyncServiceForTestCase(test_case.type, test_case.passphrase_state);

    EXPECT_EQ(test_case.expected_sync_user, IsSmartLockUser(sync_service()));
  }
}

}  // namespace password_bubble_experiment

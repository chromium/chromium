// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/enterprise_signals_disclaimer/acknowledgment_manager.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_signals_disclaimer {
namespace {

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

std::string GetHash(const GaiaId& gaia_id) {
  return signin::GaiaIdHash::FromGaiaId(gaia_id).ToBase64();
}

class AcknowledgmentManagerTest : public testing::Test {
 protected:
  void SetUp() override { RegisterLocalStatePrefs(local_state_.registry()); }

  base::flat_set<GaiaId> GetAckSet() {
    base::flat_set<GaiaId> set;
    for (const GaiaId& gaia_id : {gaia_id_1_, gaia_id_2_, gaia_id_3_}) {
      if (HasAccountAckedSignalsDisclaimer(local_state_, gaia_id)) {
        set.insert(gaia_id);
      }
    }
    return set;
  }

  TestingPrefServiceSimple local_state_;

  const GaiaId gaia_id_1_{"gaia_id_1"};
  const GaiaId gaia_id_2_{"gaia_id_2"};
  const GaiaId gaia_id_3_{"gaia_id_3"};
};

TEST_F(AcknowledgmentManagerTest, NotAcknowledgedByDefault) {
  EXPECT_FALSE(HasAccountAckedSignalsDisclaimer(local_state_, gaia_id_1_));

  const base::ListValue& ack_set =
      local_state_.GetList(kAcknowledgmentSetPrefPath);
  EXPECT_THAT(ack_set, IsEmpty());
}

TEST_F(AcknowledgmentManagerTest, SetAccountAckedSignalsDisclaimer) {
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_1_);

  EXPECT_THAT(GetAckSet(), UnorderedElementsAre(gaia_id_1_));
}

TEST_F(AcknowledgmentManagerTest, AcknowledgmentPrepopulated) {
  local_state_.SetList(kAcknowledgmentSetPrefPath,
                       base::ListValue()
                           .Append(GetHash(gaia_id_1_))
                           .Append(GetHash(gaia_id_2_)));

  EXPECT_THAT(GetAckSet(), UnorderedElementsAre(gaia_id_1_, gaia_id_2_));
}

TEST_F(AcknowledgmentManagerTest, SetMultipleAccountsAcknowledged) {
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_1_);
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_2_);

  EXPECT_THAT(GetAckSet(), UnorderedElementsAre(gaia_id_1_, gaia_id_2_));
}

TEST_F(AcknowledgmentManagerTest, SetAcknowledgmentIsIdempotent) {
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_1_);
  EXPECT_THAT(GetAckSet(), UnorderedElementsAre(gaia_id_1_));

  // Acknowledging the same account again should not create duplicate entries.
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_1_);
  EXPECT_THAT(GetAckSet(), UnorderedElementsAre(gaia_id_1_));
}

TEST_F(AcknowledgmentManagerTest,
       RemoveUnknownAccounts_EmptyAcknowledgmentSet) {
  RemoveUnknownAccounts(local_state_, {});

  EXPECT_THAT(GetAckSet(), IsEmpty());
}

TEST_F(AcknowledgmentManagerTest, RemoveUnknownAccounts_RemovesExtraAccounts) {
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_1_);
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_2_);
  EXPECT_THAT(GetAckSet(), UnorderedElementsAre(gaia_id_1_, gaia_id_2_));

  RemoveUnknownAccounts(local_state_, {gaia_id_2_});

  EXPECT_THAT(GetAckSet(), UnorderedElementsAre(gaia_id_2_));
}

TEST_F(AcknowledgmentManagerTest,
       RemoveUnknownAccounts_AccountAlreadyNotPresent) {
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_1_);
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_2_);

  RemoveUnknownAccounts(local_state_, {gaia_id_1_, gaia_id_2_});

  EXPECT_THAT(GetAckSet(), UnorderedElementsAre(gaia_id_1_, gaia_id_2_));
}

TEST_F(AcknowledgmentManagerTest,
       RemoveUnknownAccounts_NoNotificationIfNoChanges) {
  PrefChangeRegistrar registrar;
  registrar.Init(&local_state_);
  MockPrefChangeCallback observer(&local_state_);
  registrar.Add(kAcknowledgmentSetPrefPath, observer.GetCallback());

  // When the ack set pref is empty, syncing does not notify.
  EXPECT_CALL(observer, OnPreferenceChanged).Times(0);
  RemoveUnknownAccounts(local_state_, {gaia_id_1_});
  testing::Mock::VerifyAndClearExpectations(&observer);

  // When all acked accounts are still on the device, syncing does not notify.
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_1_);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnPreferenceChanged).Times(0);
  RemoveUnknownAccounts(local_state_, {gaia_id_1_, gaia_id_2_});
}

TEST_F(AcknowledgmentManagerTest, RemoveUnknownAccounts_NotifiesOnChanges) {
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_1_);
  SetAccountAckedSignalsDisclaimer(local_state_, gaia_id_2_);

  PrefChangeRegistrar registrar;
  registrar.Init(&local_state_);
  MockPrefChangeCallback observer(&local_state_);
  registrar.Add(kAcknowledgmentSetPrefPath, observer.GetCallback());

  EXPECT_CALL(observer, OnPreferenceChanged(kAcknowledgmentSetPrefPath))
      .Times(1);
  RemoveUnknownAccounts(local_state_, {gaia_id_1_});

  EXPECT_THAT(GetAckSet(), UnorderedElementsAre(gaia_id_1_));
}

}  // namespace
}  // namespace enterprise_signals_disclaimer

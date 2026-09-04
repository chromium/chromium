// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/enterprise_signals_disclaimer/acknowledgment_manager.h"

#include <string>

#include "base/values.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_signals_disclaimer {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

std::string GetHash(const GaiaId& gaia_id) {
  return signin::GaiaIdHash::FromGaiaId(gaia_id).ToBase64();
}

class AcknowledgmentManagerTest : public testing::Test {
 protected:
  void SetUp() override { RegisterLocalStatePrefs(local_state_.registry()); }

  TestingPrefServiceSimple local_state_;

  const GaiaId gaia_id_1_{"gaia_id_1"};
  const GaiaId gaia_id_2_{"gaia_id_2"};
  const GaiaId gaia_id_3_{"gaia_id_3"};
};

TEST_F(AcknowledgmentManagerTest, NotAcknowledgedByDefault) {
  EXPECT_FALSE(
      HasAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_1_));

  const base::ListValue& ack_set =
      local_state_.GetList(kAcknowledgmentSetPrefPath);
  EXPECT_THAT(ack_set, IsEmpty());
}

TEST_F(AcknowledgmentManagerTest, SetAccountAcknowledgedSignalsDisclaimer) {
  SetAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_1_);

  EXPECT_TRUE(
      HasAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_1_));
  EXPECT_FALSE(
      HasAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_2_));
}

TEST_F(AcknowledgmentManagerTest, AcknowledgmentPrepopulated) {
  local_state_.SetList(kAcknowledgmentSetPrefPath,
                       base::ListValue()
                           .Append(GetHash(gaia_id_1_))
                           .Append(GetHash(gaia_id_2_)));

  EXPECT_TRUE(
      HasAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_1_));
  EXPECT_TRUE(
      HasAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_2_));
  EXPECT_FALSE(
      HasAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_3_));
}

TEST_F(AcknowledgmentManagerTest, SetMultipleAccountsAcknowledged) {
  SetAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_1_);
  SetAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_2_);

  EXPECT_TRUE(
      HasAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_1_));
  EXPECT_TRUE(
      HasAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_2_));
  EXPECT_FALSE(
      HasAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_3_));

  const base::ListValue& ack_set =
      local_state_.GetList(kAcknowledgmentSetPrefPath);
  EXPECT_THAT(ack_set,
              UnorderedElementsAre(GetHash(gaia_id_1_), GetHash(gaia_id_2_)));
}

TEST_F(AcknowledgmentManagerTest, SetAcknowledgmentIsIdempotent) {
  SetAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_1_);
  EXPECT_THAT(local_state_.GetList(kAcknowledgmentSetPrefPath),
              UnorderedElementsAre(GetHash(gaia_id_1_)));

  // Acknowledging the same account again should not create duplicate entries.
  SetAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_1_);
  EXPECT_TRUE(
      HasAccountAcknowledgedSignalsDisclaimer(&local_state_, gaia_id_1_));
  EXPECT_THAT(local_state_.GetList(kAcknowledgmentSetPrefPath),
              UnorderedElementsAre(GetHash(gaia_id_1_)));
}

}  // namespace
}  // namespace enterprise_signals_disclaimer

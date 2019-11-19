// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "chrome/chrome_elf/third_party_dlls/beacon.h"

#include "base/strings/string16.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/chrome_elf/chrome_elf_constants.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace third_party_dlls {

namespace {

class BeaconTest : public testing::Test {
 protected:
  BeaconTest() : override_manager_() {}

  std::unique_ptr<base::win::RegKey> beacon_registry_key_;
  registry_util::RegistryOverrideManager override_manager_;

 private:
  void SetUp() override {
    base::string16 temp;
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_CURRENT_USER, &temp));
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKCU, temp));

    beacon_registry_key_.reset(
        new base::win::RegKey(HKEY_CURRENT_USER,
                              install_static::GetRegistryPath()
                                  .append(blacklist::kRegistryBeaconKeyName)
                                  .c_str(),
                              KEY_QUERY_VALUE | KEY_SET_VALUE));
  }

  void TearDown() override {
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKCU, base::string16()));
  }
};

}  // namespace

//------------------------------------------------------------------------------
// Beacon tests
//------------------------------------------------------------------------------

// Ensure that the beacon state starts off 'running' if a version is specified.
TEST_F(BeaconTest, Beacon) {
  LONG result = beacon_registry_key_->WriteValue(
      blacklist::kBeaconState, blacklist::BLACKLIST_SETUP_RUNNING);
  EXPECT_EQ(ERROR_SUCCESS, result);

  result = beacon_registry_key_->WriteValue(blacklist::kBeaconVersion,
                                            L"beacon_version");
  EXPECT_EQ(ERROR_SUCCESS, result);

  // First call should find the beacon and reset it.
  EXPECT_TRUE(ResetBeacon());

  // First call should succeed as the beacon is enabled.
  EXPECT_TRUE(LeaveSetupBeacon());
}

void TestResetBeacon(std::unique_ptr<base::win::RegKey>& key,
                     DWORD input_state,
                     DWORD expected_output_state) {
  LONG result = key->WriteValue(blacklist::kBeaconState, input_state);
  EXPECT_EQ(ERROR_SUCCESS, result);

  EXPECT_TRUE(ResetBeacon());
  DWORD blacklist_state = blacklist::BLACKLIST_STATE_MAX;
  result = key->ReadValueDW(blacklist::kBeaconState, &blacklist_state);
  EXPECT_EQ(ERROR_SUCCESS, result);
  EXPECT_EQ(expected_output_state, blacklist_state);
}

TEST_F(BeaconTest, ResetBeacon) {
  // Ensure that ResetBeacon resets properly on successful runs and not on
  // failed or disabled runs.
  TestResetBeacon(beacon_registry_key_, blacklist::BLACKLIST_SETUP_RUNNING,
                  blacklist::BLACKLIST_ENABLED);

  TestResetBeacon(beacon_registry_key_, blacklist::BLACKLIST_SETUP_FAILED,
                  blacklist::BLACKLIST_SETUP_FAILED);

  TestResetBeacon(beacon_registry_key_, blacklist::BLACKLIST_DISABLED,
                  blacklist::BLACKLIST_DISABLED);
}

TEST_F(BeaconTest, SetupFailed) {
  // Ensure that when the number of failed tries reaches the maximum allowed,
  // the blacklist state is set to failed.
  LONG result = beacon_registry_key_->WriteValue(
      blacklist::kBeaconState, blacklist::BLACKLIST_SETUP_RUNNING);
  EXPECT_EQ(ERROR_SUCCESS, result);

  // Set the attempt count so that on the next failure the blacklist is
  // disabled.
  result = beacon_registry_key_->WriteValue(blacklist::kBeaconAttemptCount,
                                            blacklist::kBeaconMaxAttempts - 1);
  EXPECT_EQ(ERROR_SUCCESS, result);

  EXPECT_FALSE(LeaveSetupBeacon());

  DWORD attempt_count = 0;
  beacon_registry_key_->ReadValueDW(blacklist::kBeaconAttemptCount,
                                    &attempt_count);
  EXPECT_EQ(attempt_count, blacklist::kBeaconMaxAttempts);

  DWORD blacklist_state = blacklist::BLACKLIST_STATE_MAX;
  result = beacon_registry_key_->ReadValueDW(blacklist::kBeaconState,
                                             &blacklist_state);
  EXPECT_EQ(ERROR_SUCCESS, result);
  EXPECT_EQ(blacklist_state,
            static_cast<DWORD>(blacklist::BLACKLIST_SETUP_FAILED));
}

TEST_F(BeaconTest, SetupSucceeded) {
  // Starting with the enabled beacon should result in the setup running state
  // and the attempt counter reset to zero.
  LONG result = beacon_registry_key_->WriteValue(blacklist::kBeaconState,
                                                 blacklist::BLACKLIST_ENABLED);
  EXPECT_EQ(ERROR_SUCCESS, result);
  result = beacon_registry_key_->WriteValue(blacklist::kBeaconAttemptCount,
                                            blacklist::kBeaconMaxAttempts);
  EXPECT_EQ(ERROR_SUCCESS, result);

  EXPECT_TRUE(LeaveSetupBeacon());

  DWORD blacklist_state = blacklist::BLACKLIST_STATE_MAX;
  beacon_registry_key_->ReadValueDW(blacklist::kBeaconState, &blacklist_state);
  EXPECT_EQ(blacklist_state,
            static_cast<DWORD>(blacklist::BLACKLIST_SETUP_RUNNING));

  DWORD attempt_count = blacklist::kBeaconMaxAttempts;
  beacon_registry_key_->ReadValueDW(blacklist::kBeaconAttemptCount,
                                    &attempt_count);
  EXPECT_EQ(static_cast<DWORD>(0), attempt_count);
}

}  // namespace third_party_dlls

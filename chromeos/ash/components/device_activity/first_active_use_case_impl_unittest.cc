// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/first_active_use_case_impl.h"

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/device_activity_controller.h"
#include "chromeos/ash/components/device_activity/fake_psm_delegate.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash {
namespace device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// Initialize fake values used by the FirstActiveUseCaseImpl.
constexpr char kFakePsmDeviceActiveSecret[] = "FAKE_PSM_DEVICE_ACTIVE_SECRET";

// TODO(hirthanan): Enable when rolling out check membership requests for the
// first active use case.
// constexpr char kHardwareClassKeyNotFound[] = "HARDWARE_CLASS_KEY_NOT_FOUND";

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

}  // namespace

class FirstActiveUseCaseImplTest : public testing::Test {
 public:
  FirstActiveUseCaseImplTest() = default;
  FirstActiveUseCaseImplTest(const FirstActiveUseCaseImplTest&) = delete;
  FirstActiveUseCaseImplTest& operator=(const FirstActiveUseCaseImplTest&) =
      delete;
  ~FirstActiveUseCaseImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    DeviceActivityController::RegisterPrefs(local_state_.registry());

    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);

    const std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids;
    first_active_use_case_impl_ = std::make_unique<FirstActiveUseCaseImpl>(
        kFakePsmDeviceActiveSecret, kFakeChromeParameters, &local_state_,
        // |FakePsmDelegate| can use any test case parameters.
        std::make_unique<FakePsmDelegate>(std::string() /* ec_cipher_key */,
                                          std::string() /* seed */,
                                          std::move(plaintext_ids)));
  }

  void TearDown() override { first_active_use_case_impl_.reset(); }

  std::unique_ptr<FirstActiveUseCaseImpl> first_active_use_case_impl_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
  chromeos::system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(FirstActiveUseCaseImplTest, ValidateWindowIdFormattedCorrectly) {
  // Create fixed timestamp used to generate a fixed window identifier.
  base::Time new_first_active_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_first_active_ts));

  std::string window_id =
      first_active_use_case_impl_->GenerateUTCWindowIdentifier(
          new_first_active_ts);

  EXPECT_EQ(window_id.size(), 8);
  EXPECT_EQ(window_id, "20220101");
}

TEST_F(FirstActiveUseCaseImplTest, SameDayTimestampsHaveSameWindowId) {
  base::Time first_active_ts_1;
  base::Time first_active_ts_2;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 00:00:00 GMT", &first_active_ts_1));
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &first_active_ts_2));

  EXPECT_EQ(first_active_use_case_impl_->GenerateUTCWindowIdentifier(
                first_active_ts_1),
            first_active_use_case_impl_->GenerateUTCWindowIdentifier(
                first_active_ts_2));
}

TEST_F(FirstActiveUseCaseImplTest,
       DifferentDayTimestampsHaveDifferentWindowId) {
  base::Time first_active_ts_1;
  base::Time first_active_ts_2;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 00:00:00 GMT", &first_active_ts_1));
  EXPECT_TRUE(
      base::Time::FromString("02 Jan 2022 00:00:00 GMT", &first_active_ts_2));

  std::string window_id_1 =
      first_active_use_case_impl_->GenerateUTCWindowIdentifier(
          first_active_ts_1);
  std::string window_id_2 =
      first_active_use_case_impl_->GenerateUTCWindowIdentifier(
          first_active_ts_2);

  EXPECT_NE(first_active_use_case_impl_->GenerateUTCWindowIdentifier(
                first_active_ts_1),
            first_active_use_case_impl_->GenerateUTCWindowIdentifier(
                first_active_ts_2));
}

}  // namespace device_activity
}  // namespace ash

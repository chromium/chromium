// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/churn_observation_use_case_impl.h"

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/churn_active_status.h"
#include "chromeos/ash/components/device_activity/device_activity_controller.h"
#include "chromeos/ash/components/device_activity/fake_psm_delegate.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// This secret should be of exactly length 64, since it is a 256 bit string
// encoded as a hexadecimal.
constexpr char kFakePsmDeviceActiveSecret[] =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

}  // namespace

class ChurnObservationUseCaseImplTest : public testing::Test {
 public:
  ChurnObservationUseCaseImplTest() = default;
  ChurnObservationUseCaseImplTest(const ChurnObservationUseCaseImplTest&) =
      delete;
  ChurnObservationUseCaseImplTest& operator=(
      const ChurnObservationUseCaseImplTest&) = delete;
  ~ChurnObservationUseCaseImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    DeviceActivityController::RegisterPrefs(local_state_.registry());

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    // Initialize the churn active status to a default value of 0.
    churn_active_status_ = std::make_unique<ChurnActiveStatus>(0);

    const std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids;
    churn_observation_use_case_impl_ =
        std::make_unique<ChurnObservationUseCaseImpl>(
            churn_active_status_.get(), kFakePsmDeviceActiveSecret,
            kFakeChromeParameters, &local_state_,
            // |FakePsmDelegate| can use any test case parameters.
            std::make_unique<FakePsmDelegate>(std::string() /* ec_cipher_key */,
                                              std::string() /* seed */,
                                              std::move(plaintext_ids)));
  }

  void TearDown() override {
    DCHECK(churn_observation_use_case_impl_);
    DCHECK(churn_active_status_);

    // Safely destruct unique pointers.
    churn_observation_use_case_impl_.reset();
    churn_active_status_.reset();
  }

  std::unique_ptr<ChurnActiveStatus> churn_active_status_;
  std::unique_ptr<ChurnObservationUseCaseImpl> churn_observation_use_case_impl_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
  system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(ChurnObservationUseCaseImplTest, ValidateWindowIdFormattedCorrectly) {
  // Create fixed timestamp used to generate a fixed window identifier.
  base::Time new_daily_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_daily_ts));

  std::string window_id =
      churn_observation_use_case_impl_->GenerateWindowIdentifier(new_daily_ts);

  EXPECT_EQ(static_cast<int>(window_id.size()), 6);
  EXPECT_EQ(window_id, "202201");
}

}  // namespace ash::device_activity

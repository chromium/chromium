// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/churn_cohort_use_case_impl.h"

#include "base/i18n/time_formatting.h"
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
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// This secret should be of exactly length 64, since it is a 256 bit string
// encoded as a hexadecimal.
constexpr char kFakePsmDeviceActiveSecret[] =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

constexpr char kHardwareClassKeyNotFound[] = "HARDWARE_CLASS_KEY_NOT_FOUND";

// This value represents the UTC based activate date of the device formatted
// YYYY-WW to reduce privacy granularity.
// See
// https://crsrc.org/o/src/third_party/chromiumos-overlay/chromeos-base/chromeos-activate-date/files/activate_date;l=67
const char kFakeFirstActivateDate[] = "2022-50";

// The decimal representation of the bit string `0100010001 000000000000001101`
// The first 10 bits represent the number of months since 2000 is 275, which
// represents the 2022-12.
// The right 18 bits represent the churn cohort active status for past 18
// months. The right most bit represents the status of previous active mont,
// in this case, it represent 2022-12. And the second right most bit
// represents 2022-11, etc.
const int kFakeChurnActiveStatus = 71565325;

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

}  // namespace

class ChurnCohortUseCaseImplTest : public testing::Test {
 public:
  ChurnCohortUseCaseImplTest() = default;
  ChurnCohortUseCaseImplTest(const ChurnCohortUseCaseImplTest&) = delete;
  ChurnCohortUseCaseImplTest& operator=(const ChurnCohortUseCaseImplTest&) =
      delete;
  ~ChurnCohortUseCaseImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    DeviceActivityController::RegisterPrefs(local_state_.registry());

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    // Set the ActiveDate key in machine statistics as kFakeFirstActivateDate.
    statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                             kFakeFirstActivateDate);

    // Initialize the churn active status to a default value of 0.
    churn_active_status_ = std::make_unique<ChurnActiveStatus>(0);

    const std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids;
    churn_cohort_use_case_impl_ = std::make_unique<ChurnCohortUseCaseImpl>(
        churn_active_status_.get(), kFakePsmDeviceActiveSecret,
        kFakeChromeParameters, &local_state_,
        // |FakePsmDelegate| can use any test case parameters.
        std::make_unique<FakePsmDelegate>(std::string() /* ec_cipher_key */,
                                          std::string() /* seed */,
                                          std::move(plaintext_ids)));
  }

  void TearDown() override {
    DCHECK(churn_cohort_use_case_impl_);
    DCHECK(churn_active_status_);

    // Safely destruct unique pointers.
    churn_cohort_use_case_impl_.reset();
    churn_active_status_.reset();
  }

  std::unique_ptr<ChurnActiveStatus> churn_active_status_;
  std::unique_ptr<ChurnCohortUseCaseImpl> churn_cohort_use_case_impl_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
  system::FakeStatisticsProvider statistics_provider_;
};

class ChurnCohortUseCaseImplEmptyFirstActiveTest : public testing::Test {
 public:
  ChurnCohortUseCaseImplEmptyFirstActiveTest() = default;
  ChurnCohortUseCaseImplEmptyFirstActiveTest(
      const ChurnCohortUseCaseImplEmptyFirstActiveTest&) = delete;
  ChurnCohortUseCaseImplEmptyFirstActiveTest& operator=(
      const ChurnCohortUseCaseImplEmptyFirstActiveTest&) = delete;
  ~ChurnCohortUseCaseImplEmptyFirstActiveTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    DeviceActivityController::RegisterPrefs(local_state_.registry());

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    // Initialize the churn active status to a default value of 0.
    churn_active_status_ = std::make_unique<ChurnActiveStatus>(0);

    const std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids;
    churn_cohort_use_case_impl_ = std::make_unique<ChurnCohortUseCaseImpl>(
        churn_active_status_.get(), kFakePsmDeviceActiveSecret,
        kFakeChromeParameters, &local_state_,
        // |FakePsmDelegate| can use any test case parameters.
        std::make_unique<FakePsmDelegate>(std::string() /* ec_cipher_key */,
                                          std::string() /* seed */,
                                          std::move(plaintext_ids)));
  }

  void TearDown() override {
    DCHECK(churn_cohort_use_case_impl_);
    DCHECK(churn_active_status_);

    // Safely destruct unique pointers.
    churn_cohort_use_case_impl_.reset();
    churn_active_status_.reset();
  }

  std::unique_ptr<ChurnActiveStatus> churn_active_status_;
  std::unique_ptr<ChurnCohortUseCaseImpl> churn_cohort_use_case_impl_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
  system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(ChurnCohortUseCaseImplTest, ValidateChurnMetadataWithFirstActiveIsTrue) {
  churn_active_status_->SetValue(kFakeChurnActiveStatus);
  base::Time new_daily_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Dec 2022 23:59:59 GMT", &new_daily_ts));
  churn_cohort_use_case_impl_->SetWindowIdentifier(new_daily_ts);
  std::string window_id_str =
      churn_cohort_use_case_impl_->GetWindowIdentifier().value();
  EXPECT_EQ(window_id_str, "202212");

  FresnelImportDataRequest req =
      churn_cohort_use_case_impl_->GenerateImportRequestBody().value();
  EXPECT_EQ(req.device_metadata().hardware_id(), kHardwareClassKeyNotFound);
  EXPECT_EQ(req.device_metadata().chromeos_channel(), Channel::CHANNEL_STABLE);
  EXPECT_EQ(req.device_metadata().market_segment(),
            MarketSegment::MARKET_SEGMENT_UNKNOWN);
  EXPECT_FALSE(req.device_metadata().chromeos_version().empty());
  EXPECT_EQ(req.import_data(0).window_identifier(), "202212");
  EXPECT_TRUE(
      req.import_data(0).churn_cohort_metadata().is_first_active_in_cohort());

  base::Time first_active_week = churn_active_status_->GetFirstActiveWeek();
  EXPECT_EQ("202212",
            base::UnlocalizedTimeFormatWithPattern(first_active_week, "yyyyMM",
                                                   icu::TimeZone::getGMT()));
}

TEST_F(ChurnCohortUseCaseImplTest, ValidateFirstActiveStatusIsFalse) {
  base::Time new_daily_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Nov 2022 23:59:59 GMT", &new_daily_ts));
  churn_cohort_use_case_impl_->SetWindowIdentifier(new_daily_ts);
  std::string window_id_str =
      churn_cohort_use_case_impl_->GetWindowIdentifier().value();
  EXPECT_EQ(window_id_str, "202211");
  FresnelImportDataRequest req =
      churn_cohort_use_case_impl_->GenerateImportRequestBody().value();
  EXPECT_FALSE(
      req.import_data(0).churn_cohort_metadata().is_first_active_in_cohort());
}

TEST_F(ChurnCohortUseCaseImplEmptyFirstActiveTest,
       ValidateFirstActiveStatusWithVPDNullField) {
  base::Time new_daily_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Dec 2022 23:59:59 GMT", &new_daily_ts));
  churn_cohort_use_case_impl_->SetWindowIdentifier(new_daily_ts);
  std::string window_id_str =
      churn_cohort_use_case_impl_->GetWindowIdentifier().value();
  EXPECT_EQ(window_id_str, "202212");
  FresnelImportDataRequest req =
      churn_cohort_use_case_impl_->GenerateImportRequestBody().value();
  EXPECT_FALSE(req.import_data(0)
                   .churn_cohort_metadata()
                   .has_is_first_active_in_cohort());

  // First active week should be null.
  EXPECT_TRUE(churn_active_status_->GetFirstActiveWeek() == base::Time());
}
}  // namespace ash::device_activity

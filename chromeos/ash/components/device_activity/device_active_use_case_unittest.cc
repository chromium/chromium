// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/device_active_use_case.h"

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/daily_use_case_impl.h"
#include "chromeos/ash/components/device_activity/device_activity_controller.h"
#include "chromeos/ash/components/device_activity/fake_psm_delegate.h"
#include "chromeos/ash/components/device_activity/first_active_use_case_impl.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/device_activity/monthly_use_case_impl.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// Initialize the constant used to represent the first active window identifier.
constexpr char kFirstActiveWindowIdentifier[] = "FIRST_ACTIVE";

// Initialize fake value used by the FirstActiveUseCaseImpl.
// This secret should be of exactly length 64, since it is a 256 bit string
// encoded as a hexadecimal.
constexpr char kFakePsmDeviceActiveSecret[] =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

// PSM expects length of seed to be of size 32.
constexpr char kFakePsmSeed[] = "00000000000000000000000000000000";
constexpr char kFakeEcCipherKey[] = "FAKE_EC_CIPHER_KEY";

}  // namespace

class DeviceActiveUseCaseTest : public testing::Test {
 public:
  DeviceActiveUseCaseTest() = default;
  DeviceActiveUseCaseTest(const DeviceActiveUseCaseTest&) = delete;
  DeviceActiveUseCaseTest& operator=(const DeviceActiveUseCaseTest&) = delete;
  ~DeviceActiveUseCaseTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    DeviceActivityController::RegisterPrefs(local_state_.registry());

    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);

    const std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids;

    // Each specific use case that will be unit tested is added here.
    use_cases_.push_back(std::make_unique<DailyUseCaseImpl>(
        kFakePsmDeviceActiveSecret, kFakeChromeParameters, &local_state_,
        // |FakePsmDelegate| can use any test case parameters.
        std::make_unique<FakePsmDelegate>(kFakeEcCipherKey, kFakePsmSeed,
                                          plaintext_ids)));
    use_cases_.push_back(std::make_unique<MonthlyUseCaseImpl>(
        kFakePsmDeviceActiveSecret, kFakeChromeParameters, &local_state_,
        // |FakePsmDelegate| can use any test case parameters.
        std::make_unique<FakePsmDelegate>(kFakeEcCipherKey, kFakePsmSeed,
                                          plaintext_ids)));
    use_cases_.push_back(std::make_unique<FirstActiveUseCaseImpl>(
        kFakePsmDeviceActiveSecret, kFakeChromeParameters, &local_state_,
        // |FakePsmDelegate| can use any test case parameters.
        std::make_unique<FakePsmDelegate>(kFakeEcCipherKey, kFakePsmSeed,
                                          plaintext_ids)));
  }

  void TearDown() override { use_cases_.clear(); }

  std::vector<std::unique_ptr<DeviceActiveUseCase>> use_cases_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
  chromeos::system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(DeviceActiveUseCaseTest, ClearSavedState) {
  // Create fixed timestamp to see if local state updates value correctly.
  base::Time new_ts;
  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT", &new_ts));

  for (auto& use_case : use_cases_) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_TRUE(use_case->SetWindowIdentifier(new_ts));

    EXPECT_TRUE(use_case->GetWindowIdentifier().has_value());
    EXPECT_TRUE(use_case->GetPsmIdentifier().has_value());
    EXPECT_THAT(use_case->GetPsmRlweClient(), testing::IsNull());

    use_case->ClearSavedState();

    EXPECT_FALSE(use_case->GetWindowIdentifier().has_value());
    EXPECT_FALSE(use_case->GetPsmIdentifier().has_value());
    EXPECT_THAT(use_case->GetPsmRlweClient(), testing::IsNull());
  }
}

TEST_F(DeviceActiveUseCaseTest, CheckIfLastKnownPingTimestampNotSet) {
  for (auto& use_case : use_cases_) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));
    EXPECT_FALSE(use_case->IsLastKnownPingTimestampSet());
  }
}

TEST_F(DeviceActiveUseCaseTest, CheckIfLastKnownPingTimestampSet) {
  // Create fixed timestamp to used to update local state timestamps.
  base::Time new_ts;
  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_ts));

  for (auto& use_case : use_cases_) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));
    // Update local state with fixed timestamp.
    use_case->SetLastKnownPingTimestamp(new_ts);

    EXPECT_EQ(use_case->GetLastKnownPingTimestamp(), new_ts);
    EXPECT_TRUE(use_case->IsLastKnownPingTimestampSet());
  }
}

TEST_F(DeviceActiveUseCaseTest, CheckPsmIdEmptyIfWindowIdIsNotSet) {
  for (auto& use_case : use_cases_) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // |use_case| must set the window id before generating the psm
    // id.
    EXPECT_THAT(use_case->GetPsmIdentifier(), testing::Eq(absl::nullopt));
  }
}

TEST_F(DeviceActiveUseCaseTest, CheckPsmIdGeneratedCorrectly) {
  // Create fixed timestamp used to generate a fixed window identifier.
  // The window id must be set before generating the psm id.
  base::Time new_ts;
  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_ts));

  for (auto& use_case : use_cases_) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    use_case->SetWindowIdentifier(new_ts);

    absl::optional<psm_rlwe::RlwePlaintextId> psm_id =
        use_case->GetPsmIdentifier();

    EXPECT_TRUE(psm_id.has_value());

    // Verify the PSM value is correct for parameters supplied by the unit
    // tests.
    std::string unhashed_psm_id =
        base::JoinString({psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()),
                          use_case->GenerateUTCWindowIdentifier(new_ts)},
                         "|");
    std::string expected_psm_id_hex =
        use_case->GetDigestString(kFakePsmDeviceActiveSecret, unhashed_psm_id);
    EXPECT_EQ(psm_id.value().sensitive_id(), expected_psm_id_hex);
  }
}

TEST_F(DeviceActiveUseCaseTest, PingRequiredInNonOverlappingUTCWindows) {
  base::Time last_ts;
  base::Time current_ts;

  for (auto& use_case : use_cases_) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // Set the last and current timestamps depending on the use case.
    switch (use_case->GetPsmUseCase()) {
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY:
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 00:00:00 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("02 Jan 2022 00:00:00 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_MONTHLY:
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 00:00:00 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("01 Feb 2022 00:00:00 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE:
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 00:00:00 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("02 Jan 2022 00:00:00 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      default:
        NOTREACHED() << "Unsupported PSM use case";
    }

    EXPECT_TRUE(use_case->IsDevicePingRequired(current_ts));
  }
}

TEST_F(DeviceActiveUseCaseTest, PingNotRequiredInOverlappingUTCWindows) {
  base::Time last_ts;
  base::Time current_ts;

  for (auto& use_case : use_cases_) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // Set the last and current timestamps depending on the use case.
    switch (use_case->GetPsmUseCase()) {
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY:
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 00:00:00 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 23:59:59 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_MONTHLY:
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 00:00:00 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("31 Jan 2022 23:59:59 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE:
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 00:00:00 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 23:59:59 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      default:
        NOTREACHED() << "Unsupported PSM use case";
    }

    // The first active use case ping is always required. This is because the
    // window identifier is constant, and does not depend on any timestamp,
    // meaning the default behaviour would result in |IsDevicePingRequired|
    // always returning false..
    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE)
      EXPECT_TRUE(use_case->IsDevicePingRequired(current_ts));
    else
      EXPECT_FALSE(use_case->IsDevicePingRequired(current_ts));
  }
}

TEST_F(DeviceActiveUseCaseTest, CheckPingRequiredInUTCBoundaryCases) {
  base::Time last_ts;
  base::Time current_ts;

  for (auto& use_case : use_cases_) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // Set the last and current timestamps depending on the use case.
    switch (use_case->GetPsmUseCase()) {
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY:
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 23:59:59 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("02 Jan 2022 00:00:00 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_MONTHLY:
        EXPECT_TRUE(
            base::Time::FromString("31 Jan 2022 23:59:59 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("01 Feb 2022 00:00:00 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE:
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 23:59:59 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("02 Jan 2022 00:00:00 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      default:
        NOTREACHED() << "Unsupported PSM use case";
    }

    EXPECT_TRUE(use_case->IsDevicePingRequired(current_ts));
  }
}

TEST_F(DeviceActiveUseCaseTest, PingNotRequiredWhenLastTimeAheadOfCurrentTime) {
  base::Time last_ts;
  base::Time current_ts;

  for (auto& use_case : use_cases_) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // Set the last and current timestamps depending on the use case.
    switch (use_case->GetPsmUseCase()) {
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY:
        EXPECT_TRUE(
            base::Time::FromString("02 Jan 2022 00:00:00 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 23:59:59 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_MONTHLY:
        EXPECT_TRUE(
            base::Time::FromString("01 Feb 2022 00:00:00 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("31 Jan 2022 23:59:59 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      case psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE:
        EXPECT_TRUE(
            base::Time::FromString("02 Jan 2022 00:00:00 GMT", &last_ts));
        EXPECT_TRUE(
            base::Time::FromString("01 Jan 2022 23:59:59 GMT", &current_ts));

        use_case->SetLastKnownPingTimestamp(last_ts);
        break;
      default:
        NOTREACHED() << "Unsupported PSM use case";
    }

    // The first active use case ping is always required. This is because the
    // window identifier is constant, and does not depend on any timestamp,
    // meaning the default behaviour would result in |IsDevicePingRequired|
    // always returning false..
    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE)
      EXPECT_TRUE(use_case->IsDevicePingRequired(current_ts));
    else
      EXPECT_FALSE(use_case->IsDevicePingRequired(current_ts));
  }
}

TEST_F(DeviceActiveUseCaseTest, SameWindowIdGeneratesSamePsmId) {
  // For simplicity, set |ts_2| as a value that is larger than the largest
  // use case window.
  base::Time ts;

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT", &ts));

  for (auto& use_case : use_cases_) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // Generates the window identifier, psm identifier, and psm rlwe client.
    use_case->SetWindowIdentifier(ts);
    absl::optional<psm_rlwe::RlwePlaintextId> psm_id_1 =
        use_case->GetPsmIdentifier();

    // Resets all saved state in the use case.
    // i.e the window identifier, psm identifier, and psm rlwe client will get
    // reset.
    use_case->ClearSavedState();

    // Regenerate the window identifier, psm identifier and psm rlwe client
    // using the same ts.
    use_case->SetWindowIdentifier(ts);
    absl::optional<psm_rlwe::RlwePlaintextId> psm_id_2 =
        use_case->GetPsmIdentifier();

    EXPECT_EQ(psm_id_1.value().sensitive_id(), psm_id_2.value().sensitive_id());
  }
}

TEST_F(DeviceActiveUseCaseTest, DifferentWindowIdGeneratesDifferentPsmId) {
  // For simplicity, set |ts_2| as a value that is larger than the largest
  // use case window.
  base::Time ts_1;
  base::Time ts_2;

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT", &ts_1));
  EXPECT_TRUE(base::Time::FromString("01 Jan 2023 00:00:00 GMT", &ts_2));

  for (auto& use_case : use_cases_) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    use_case->SetWindowIdentifier(ts_1);
    absl::optional<psm_rlwe::RlwePlaintextId> psm_id_1 =
        use_case->GetPsmIdentifier();

    use_case->SetWindowIdentifier(ts_2);
    absl::optional<psm_rlwe::RlwePlaintextId> psm_id_2 =
        use_case->GetPsmIdentifier();

    // The first active use case always generates a single PSM ID.
    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE)
      EXPECT_EQ(psm_id_1.value().sensitive_id(),
                psm_id_2.value().sensitive_id());
    else
      EXPECT_NE(psm_id_1.value().sensitive_id(),
                psm_id_2.value().sensitive_id());
  }
}

TEST_F(DeviceActiveUseCaseTest, NonFirstActiveWindowIdsDependOnTimestamp) {
  base::Time ts_1;
  EXPECT_TRUE(base::Time::FromString("01 Jan 2023 00:00:00 GMT", &ts_1));

  for (auto& use_case : use_cases_) {
    if (use_case->GetPsmUseCase() !=
        psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE) {
      SCOPED_TRACE(testing::Message()
                   << "PSM use case: "
                   << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

      // For use cases other than first active, the generated window identifier
      // depends on the timestamp passed to the method.
      EXPECT_NE(use_case->GenerateUTCWindowIdentifier(base::Time::UnixEpoch()),
                use_case->GenerateUTCWindowIdentifier(ts_1));
    }
  }
}

TEST_F(DeviceActiveUseCaseTest, FirstActiveWindowIdIsAlwaysConstant) {
  base::Time ts_1;
  EXPECT_TRUE(base::Time::FromString("01 Jan 2023 00:00:00 GMT", &ts_1));

  for (auto& use_case : use_cases_) {
    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE) {
      SCOPED_TRACE(testing::Message()
                   << "PSM use case: "
                   << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

      // Different timestamps passed to first active use case
      // |GenerateUTCWindowIdentifier| method output the same constant string.
      EXPECT_EQ(use_case->GenerateUTCWindowIdentifier(base::Time::UnixEpoch()),
                kFirstActiveWindowIdentifier);
      EXPECT_EQ(use_case->GenerateUTCWindowIdentifier(ts_1),
                kFirstActiveWindowIdentifier);
    }
  }
}

TEST_F(DeviceActiveUseCaseTest, FirstActiveDevicePingIsAlwaysRequired) {
  base::Time ts_1;
  EXPECT_TRUE(base::Time::FromString("01 Jan 2023 00:00:00 GMT", &ts_1));

  for (auto& use_case : use_cases_) {
    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE) {
      SCOPED_TRACE(testing::Message()
                   << "PSM use case: "
                   << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

      EXPECT_TRUE(use_case->IsDevicePingRequired(ts_1));
    }
  }
}

}  // namespace ash::device_activity

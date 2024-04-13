// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"

#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/prefs/testing_pref_service.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::phonehub {

class PhoneHubStructuredMetricsLoggerTest : public testing::Test {
 protected:
  PhoneHubStructuredMetricsLoggerTest() = default;
  PhoneHubStructuredMetricsLoggerTest(
      const PhoneHubStructuredMetricsLoggerTest&) = delete;
  PhoneHubStructuredMetricsLoggerTest& operator=(
      const PhoneHubStructuredMetricsLoggerTest&) = delete;
  ~PhoneHubStructuredMetricsLoggerTest() override = default;

  void SetUp() override {
    PhoneHubStructuredMetricsLogger::RegisterPrefs(pref_service_.registry());
    feature_list_.InitWithFeatures(
        {metrics::structured::kPhoneHubStructuredMetrics}, {});
    logger_ = std::make_unique<PhoneHubStructuredMetricsLogger>(&pref_service_);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<PhoneHubStructuredMetricsLogger> logger_;
  TestingPrefServiceSimple pref_service_;
  network_config::CrosNetworkConfigTestHelper network_config_test_helper_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PhoneHubStructuredMetricsLoggerTest,
       ProcessPhoneInformation_MissingFields) {
  auto phone_property = proto::PhoneProperties();
  phone_property.set_android_version(32);
  phone_property.set_gmscore_version(11111111);
  phone_property.set_profile_type(proto::ProfileType::DEFAULT_PROFILE);

  logger_->ProcessPhoneInformation(phone_property);

  EXPECT_TRUE(
      pref_service_.GetTime(prefs::kPseudonymousIdRotationDate).is_null());
  EXPECT_TRUE(pref_service_.GetString(prefs::kPhonePseudonymousId).empty());
  EXPECT_TRUE(pref_service_.GetString(prefs::kPhoneManufacturer).empty());
  EXPECT_TRUE(pref_service_.GetString(prefs::kPhoneModel).empty());
  EXPECT_TRUE(pref_service_.GetString(prefs::kPhoneLocale).empty());
  EXPECT_EQ(pref_service_.GetInt64(prefs::kPhoneGmsCoreVersion), 11111111);
  EXPECT_EQ(pref_service_.GetInteger(prefs::kPhoneAndroidVersion), 32);
  EXPECT_EQ(pref_service_.GetInteger(prefs::kPhoneProfileType),
            proto::ProfileType::DEFAULT_PROFILE);
  EXPECT_EQ(pref_service_.GetInt64(prefs::kPhoneAmbientApkVersion), 0);
  EXPECT_EQ(logger_->network_state_, NetworkState::kUnknown);
}

TEST_F(PhoneHubStructuredMetricsLoggerTest, ProcessPhoneInformation_AllFields) {
  int id_rotation_time_in_milliseconds = 123456789;
  int phone_android_version = 32;
  int gms_version = 11111111;
  int ambient_version = 1234567;

  auto phone_property = proto::PhoneProperties();
  phone_property.set_android_version(phone_android_version);
  phone_property.set_gmscore_version(gms_version);
  phone_property.set_profile_type(proto::ProfileType::DEFAULT_PROFILE);
  phone_property.set_ambient_version(ambient_version);
  phone_property.set_phone_pseudonymous_id("test_uuid");
  phone_property.set_pseudonymous_id_next_rotation_date(
      id_rotation_time_in_milliseconds);
  phone_property.set_locale("US");
  phone_property.set_phone_manufacturer("google");
  phone_property.set_phone_model("pixle 7");
  phone_property.set_network_status(proto::NetworkStatus::CELLULAR);

  logger_->ProcessPhoneInformation(phone_property);

  EXPECT_EQ(pref_service_.GetTime(prefs::kPseudonymousIdRotationDate),
            base::Time::FromMillisecondsSinceUnixEpoch(
                id_rotation_time_in_milliseconds));
  EXPECT_EQ(pref_service_.GetString(prefs::kPhonePseudonymousId), "test_uuid");
  EXPECT_EQ(pref_service_.GetString(prefs::kPhoneManufacturer), "google");
  EXPECT_EQ(pref_service_.GetString(prefs::kPhoneModel), "pixle 7");
  EXPECT_EQ(pref_service_.GetString(prefs::kPhoneLocale), "US");
  EXPECT_EQ(pref_service_.GetInt64(prefs::kPhoneGmsCoreVersion), gms_version);
  EXPECT_EQ(pref_service_.GetInteger(prefs::kPhoneAndroidVersion),
            phone_android_version);
  EXPECT_EQ(pref_service_.GetInteger(prefs::kPhoneProfileType),
            proto::ProfileType::DEFAULT_PROFILE);
  EXPECT_EQ(pref_service_.GetInt64(prefs::kPhoneAmbientApkVersion),
            ambient_version);
  EXPECT_EQ(logger_->network_state_, NetworkState::kPhoneOnCellular);

  // Simulate phone info update
  int update_id_rotation_time_in_milliseconds = 12345678;
  int update_phone_android_version = 34;
  int update_gms_version = 111111112;
  int update_ambient_version = 123456777;

  phone_property.set_android_version(update_phone_android_version);
  phone_property.set_gmscore_version(update_gms_version);
  phone_property.set_profile_type(proto::ProfileType::DEFAULT_PROFILE);
  phone_property.set_ambient_version(update_ambient_version);
  phone_property.set_phone_pseudonymous_id("test_uuid_2");
  phone_property.set_pseudonymous_id_next_rotation_date(
      update_id_rotation_time_in_milliseconds);
  phone_property.set_locale("us");
  phone_property.set_phone_manufacturer("Google");
  phone_property.set_phone_model("Pixle 7");
  phone_property.set_network_status(proto::NetworkStatus::WIFI);
  phone_property.set_ssid(crypto::SHA256HashString("WIFI1"));
  phone_property.set_profile_type(proto::ProfileType::WORK_PROFILE);

  auto wifi_path =
      network_config_test_helper_.network_state_helper().ConfigureService(
          R"({"GUID": "WIFI1_guid", "Type": "wifi", "SSID": "WIFI1",
             "State": "ready", "Strength": 100,
            "Connectable": true})");
  base::RunLoop().RunUntilIdle();
  logger_->ProcessPhoneInformation(phone_property);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(pref_service_.GetTime(prefs::kPseudonymousIdRotationDate),
            base::Time::FromMillisecondsSinceUnixEpoch(
                update_id_rotation_time_in_milliseconds));
  EXPECT_EQ(pref_service_.GetString(prefs::kPhonePseudonymousId),
            "test_uuid_2");
  EXPECT_EQ(pref_service_.GetString(prefs::kPhoneManufacturer), "Google");
  EXPECT_EQ(pref_service_.GetString(prefs::kPhoneModel), "Pixle 7");
  EXPECT_EQ(pref_service_.GetString(prefs::kPhoneLocale), "us");
  EXPECT_EQ(pref_service_.GetInt64(prefs::kPhoneGmsCoreVersion),
            update_gms_version);
  EXPECT_EQ(pref_service_.GetInteger(prefs::kPhoneAndroidVersion),
            update_phone_android_version);
  EXPECT_EQ(pref_service_.GetInteger(prefs::kPhoneProfileType),
            proto::ProfileType::WORK_PROFILE);
  EXPECT_EQ(pref_service_.GetInt64(prefs::kPhoneAmbientApkVersion),
            update_ambient_version);
  EXPECT_EQ(logger_->network_state_, NetworkState::kSameNetwork);

  // Simulate phone wifi change
  phone_property.set_ssid(crypto::SHA256HashString("WIFI2"));
  logger_->ProcessPhoneInformation(phone_property);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(logger_->network_state_, NetworkState::kDifferentNetwork);
}

TEST_F(PhoneHubStructuredMetricsLoggerTest, LogEvents) {
  logger_->LogPhoneHubDiscoveryStarted(
      DiscoveryEntryPoint::kPhoneHubBubbleOpen);

  int id_rotation_time_in_milliseconds =
      base::Time::Now().InMillisecondsSinceUnixEpoch() +
      5 * 24 * 60 * 60 * 1000;
  int phone_android_version = 32;
  int gms_version = 11111111;
  int ambient_version = 1234567;
  auto phone_property = proto::PhoneProperties();
  phone_property.set_android_version(phone_android_version);
  phone_property.set_gmscore_version(gms_version);
  phone_property.set_profile_type(proto::ProfileType::DEFAULT_PROFILE);
  phone_property.set_ambient_version(ambient_version);
  phone_property.set_phone_pseudonymous_id("test_uuid");
  phone_property.set_pseudonymous_id_next_rotation_date(
      id_rotation_time_in_milliseconds);
  phone_property.set_locale("US");
  phone_property.set_phone_manufacturer("google");
  phone_property.set_phone_model("pixle 7");
  phone_property.set_network_status(proto::NetworkStatus::CELLULAR);

  logger_->ProcessPhoneInformation(phone_property);

  std::string chromebook_pseudonymouse_id =
      pref_service_.GetString(prefs::kChromebookPseudonymousId);
  EXPECT_FALSE(chromebook_pseudonymouse_id.empty());
  std::string phone_hub_session_id = logger_->phone_hub_session_id_;
  EXPECT_FALSE(phone_hub_session_id.empty());

  logger_->LogDiscoveryAttempt(secure_channel::mojom::DiscoveryResult::kSuccess,
                               std::nullopt);
  EXPECT_EQ(chromebook_pseudonymouse_id,
            pref_service_.GetString(prefs::kChromebookPseudonymousId));
  EXPECT_EQ(phone_hub_session_id, logger_->phone_hub_session_id_);

  logger_->LogNearbyConnectionState(
      secure_channel::mojom::NearbyConnectionStep::kDisconnectionStarted,
      secure_channel::mojom::NearbyConnectionStepResult::kSuccess);
  EXPECT_EQ(chromebook_pseudonymouse_id,
            pref_service_.GetString(prefs::kChromebookPseudonymousId));
  EXPECT_EQ(phone_hub_session_id, logger_->phone_hub_session_id_);

  logger_->LogSecureChannelState(
      secure_channel::mojom::SecureChannelState::kAuthenticationSuccess);
  EXPECT_EQ(chromebook_pseudonymouse_id,
            pref_service_.GetString(prefs::kChromebookPseudonymousId));
  EXPECT_EQ(phone_hub_session_id, logger_->phone_hub_session_id_);

  logger_->LogPhoneHubUiStateUpdated(PhoneHubUiState::kConnected);
  EXPECT_EQ(chromebook_pseudonymouse_id,
            pref_service_.GetString(prefs::kChromebookPseudonymousId));
  EXPECT_EQ(phone_hub_session_id, logger_->phone_hub_session_id_);

  logger_->LogPhoneHubMessageEvent(
      proto::MessageType::PHONE_STATUS_SNAPSHOT,
      PhoneHubMessageDirection::kPhoneToChromebook);
  EXPECT_EQ(chromebook_pseudonymouse_id,
            pref_service_.GetString(prefs::kChromebookPseudonymousId));
  EXPECT_EQ(phone_hub_session_id, logger_->phone_hub_session_id_);

  task_environment_.FastForwardBy(base::Days(6));
  logger_->LogDiscoveryAttempt(secure_channel::mojom::DiscoveryResult::kSuccess,
                               std::nullopt);
  EXPECT_FALSE(chromebook_pseudonymouse_id ==
               pref_service_.GetString(prefs::kChromebookPseudonymousId));
  EXPECT_FALSE(phone_hub_session_id == logger_->phone_hub_session_id_);
  EXPECT_TRUE(pref_service_.GetString(prefs::kPhonePseudonymousId).empty());
  EXPECT_TRUE(pref_service_.GetString(prefs::kPhoneManufacturer).empty());
  EXPECT_TRUE(pref_service_.GetString(prefs::kPhoneModel).empty());
  EXPECT_TRUE(pref_service_.GetString(prefs::kPhoneLocale).empty());
  EXPECT_EQ(pref_service_.GetInt64(prefs::kPhoneGmsCoreVersion), 0);
  EXPECT_EQ(pref_service_.GetInteger(prefs::kPhoneAndroidVersion), 0);
  EXPECT_EQ(pref_service_.GetInteger(prefs::kPhoneProfileType), -1);
  EXPECT_EQ(pref_service_.GetInt64(prefs::kPhoneAmbientApkVersion), 0);
  EXPECT_EQ(logger_->network_state_, NetworkState::kUnknown);
}

}  // namespace ash::phonehub

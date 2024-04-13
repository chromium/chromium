// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"

#include <string>

#include "ash/public/cpp/network_config_service.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/system/sys_info.h"
#include "base/uuid.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "crypto/sha2.h"
#include "device/bluetooth/floss/floss_features.h"

namespace ash::phonehub {

void PhoneHubStructuredMetricsLogger::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kPhoneManufacturer, std::string());
  registry->RegisterStringPref(prefs::kPhoneModel, std::string());
  registry->RegisterStringPref(prefs::kPhoneLocale, std::string());
  registry->RegisterStringPref(prefs::kPhonePseudonymousId, std::string());
  registry->RegisterInt64Pref(prefs::kPhoneAmbientApkVersion, 0);
  registry->RegisterInt64Pref(prefs::kPhoneGmsCoreVersion, 0);
  registry->RegisterIntegerPref(prefs::kPhoneAndroidVersion, 0);
  registry->RegisterIntegerPref(
      prefs::kPhoneProfileType,
      -1);  // Make default value different from normal default profile type 0
  registry->RegisterTimePref(prefs::kPhoneInfoLastUpdatedTime, base::Time());
  registry->RegisterStringPref(prefs::kChromebookPseudonymousId, std::string());
  registry->RegisterTimePref(prefs::kPseudonymousIdRotationDate, base::Time());
}

PhoneHubStructuredMetricsLogger::PhoneHubStructuredMetricsLogger(
    PrefService* pref_service)
    : bluetooth_stack_(floss::features::IsFlossEnabled()
                           ? BluetoothStack::kFloss
                           : BluetoothStack::kBlueZ),
      chromebook_locale_(base::i18n::GetConfiguredLocale()),
      pref_service_(pref_service) {
  ash::GetNetworkConfigService(
      cros_network_config_.BindNewPipeAndPassReceiver());
}
PhoneHubStructuredMetricsLogger::~PhoneHubStructuredMetricsLogger() = default;

void PhoneHubStructuredMetricsLogger::LogPhoneHubDiscoveryStarted(
    DiscoveryEntryPoint entry_point) {
  if (!base::FeatureList::IsEnabled(
          metrics::structured::kPhoneHubStructuredMetrics)) {
    return;
  }
  UpdateIdentifiersIfNeeded();
}

void PhoneHubStructuredMetricsLogger::LogDiscoveryAttempt(
    secure_channel::mojom::DiscoveryResult result,
    std::optional<secure_channel::mojom::DiscoveryErrorCode> error_code) {
  if (!base::FeatureList::IsEnabled(
          metrics::structured::kPhoneHubStructuredMetrics)) {
    return;
  }
  UpdateIdentifiersIfNeeded();
}

void PhoneHubStructuredMetricsLogger::LogNearbyConnectionState(
    secure_channel::mojom::NearbyConnectionStep step,
    secure_channel::mojom::NearbyConnectionStepResult result) {
  if (!base::FeatureList::IsEnabled(
          metrics::structured::kPhoneHubStructuredMetrics)) {
    return;
  }
  UpdateIdentifiersIfNeeded();
}

void PhoneHubStructuredMetricsLogger::LogSecureChannelState(
    secure_channel::mojom::SecureChannelState state) {
  if (!base::FeatureList::IsEnabled(
          metrics::structured::kPhoneHubStructuredMetrics)) {
    return;
  }
  UpdateIdentifiersIfNeeded();
}

void PhoneHubStructuredMetricsLogger::LogPhoneHubMessageEvent(
    proto::MessageType message_type,
    PhoneHubMessageDirection message_direction) {
  if (!base::FeatureList::IsEnabled(
          metrics::structured::kPhoneHubStructuredMetrics)) {
    return;
  }
  UpdateIdentifiersIfNeeded();
}

void PhoneHubStructuredMetricsLogger::LogPhoneHubUiStateUpdated(
    PhoneHubUiState ui_state) {
  if (!base::FeatureList::IsEnabled(
          metrics::structured::kPhoneHubStructuredMetrics)) {
    return;
  }
  UpdateIdentifiersIfNeeded();
}

void PhoneHubStructuredMetricsLogger::ProcessPhoneInformation(
    const proto::PhoneProperties& phone_properties) {
  if (!base::FeatureList::IsEnabled(
          metrics::structured::kPhoneHubStructuredMetrics)) {
    return;
  }
  if (phone_properties.has_pseudonymous_id_next_rotation_date()) {
    base::Time pseudonymous_id_rotation_date =
        base::Time::FromMillisecondsSinceUnixEpoch(
            phone_properties.pseudonymous_id_next_rotation_date());
    if (pref_service_->GetTime(prefs::kPseudonymousIdRotationDate).is_null() ||
        pseudonymous_id_rotation_date <
            pref_service_->GetTime(prefs::kPseudonymousIdRotationDate)) {
      pref_service_->SetTime(prefs::kPseudonymousIdRotationDate,
                             pseudonymous_id_rotation_date);
    }
  }
  if (phone_properties.has_phone_pseudonymous_id()) {
    if (pref_service_->GetString(prefs::kPhonePseudonymousId).empty() ||
        pref_service_->GetString(prefs::kPhonePseudonymousId) !=
            phone_properties.phone_pseudonymous_id()) {
      pref_service_->SetString(prefs::kPhonePseudonymousId,
                               phone_properties.phone_pseudonymous_id());
    }
  }
  if (phone_properties.has_phone_manufacturer()) {
    if (pref_service_->GetString(prefs::kPhoneManufacturer).empty() ||
        pref_service_->GetString(prefs::kPhoneManufacturer) !=
            phone_properties.phone_manufacturer()) {
      pref_service_->SetString(prefs::kPhoneManufacturer,
                               phone_properties.phone_manufacturer());
    }
  }
  if (phone_properties.has_phone_model()) {
    if (pref_service_->GetString(prefs::kPhoneModel).empty() ||
        pref_service_->GetString(prefs::kPhoneModel) !=
            phone_properties.phone_model()) {
      pref_service_->SetString(prefs::kPhoneModel,
                               phone_properties.phone_model());
    }
  }

  if (pref_service_->GetInt64(prefs::kPhoneGmsCoreVersion) !=
      phone_properties.gmscore_version()) {
    pref_service_->SetInt64(prefs::kPhoneGmsCoreVersion,
                            phone_properties.gmscore_version());
  }

  if (pref_service_->GetInteger(prefs::kPhoneAndroidVersion) !=
      phone_properties.android_version()) {
    pref_service_->SetInteger(prefs::kPhoneAndroidVersion,
                              phone_properties.android_version());
  }

  if (phone_properties.has_ambient_version() &&
      pref_service_->GetInt64(prefs::kPhoneAmbientApkVersion) !=
          phone_properties.ambient_version()) {
    pref_service_->SetInt64(prefs::kPhoneAmbientApkVersion,
                            phone_properties.ambient_version());
  }

  pref_service_->SetTime(prefs::kPhoneInfoLastUpdatedTime,
                         base::Time::NowFromSystemTime());

  if (phone_properties.has_network_status()) {
    phone_network_status_ = phone_properties.network_status();
    if (phone_network_status_ == proto::NetworkStatus::CELLULAR) {
      network_state_ = NetworkState::kPhoneOnCellular;
    } else if (phone_network_status_ == proto::NetworkStatus::WIFI) {
      if (phone_properties.has_ssid()) {
        phone_network_ssid_ = phone_properties.ssid();
      }
      cros_network_config_->GetNetworkStateList(
          chromeos::network_config::mojom::NetworkFilter::New(
              chromeos::network_config::mojom::FilterType::kActive,
              chromeos::network_config::mojom::NetworkType::kWiFi,
              chromeos::network_config::mojom::kNoLimit),
          base::BindOnce(
              &PhoneHubStructuredMetricsLogger::OnNetworkStateListFetched,
              base::Unretained(this)));
    } else {
      network_state_ = NetworkState::kDifferentNetwork;
    }
  }

  if (pref_service_->GetInteger(prefs::kPhoneProfileType) !=
      phone_properties.profile_type()) {
    pref_service_->SetInteger(prefs::kPhoneProfileType,
                              phone_properties.profile_type());
  }

  if (phone_properties.has_locale()) {
    if (pref_service_->GetString(prefs::kPhoneLocale).empty() ||
        pref_service_->GetString(prefs::kPhoneLocale) !=
            phone_properties.locale()) {
      pref_service_->SetString(prefs::kPhoneLocale, phone_properties.locale());
    }
  }
}

void PhoneHubStructuredMetricsLogger::UpdateIdentifiersIfNeeded() {
  if (!base::FeatureList::IsEnabled(
          metrics::structured::kPhoneHubStructuredMetrics)) {
    return;
  }
  if (pref_service_->GetTime(prefs::kPseudonymousIdRotationDate).is_null() ||
      pref_service_->GetTime(prefs::kPseudonymousIdRotationDate) <=
          base::Time::NowFromSystemTime()) {
    ResetCachedInformation();
  }
  if (pref_service_->GetString(prefs::kChromebookPseudonymousId).empty()) {
    pref_service_->SetString(
        prefs::kChromebookPseudonymousId,
        base::Uuid::GenerateRandomV4().AsLowercaseString());
  }
  if (pref_service_->GetTime(prefs::kPseudonymousIdRotationDate).is_null() ||
      pref_service_->GetTime(prefs::kPseudonymousIdRotationDate) >
          (base::Time::NowFromSystemTime() +
           kMaxStructuredMetricsPseudonymousIdDays)) {
    pref_service_->SetTime(prefs::kPseudonymousIdRotationDate,
                           base::Time::NowFromSystemTime() +
                               kMaxStructuredMetricsPseudonymousIdDays);
  }
  if (phone_hub_session_id_.empty()) {
    phone_hub_session_id_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
  }
}

void PhoneHubStructuredMetricsLogger::ResetCachedInformation() {
  if (!base::FeatureList::IsEnabled(
          metrics::structured::kPhoneHubStructuredMetrics)) {
    return;
  }
  phone_network_status_ = std::nullopt;
  pref_service_->SetInteger(prefs::kPhoneProfileType, -1);
  phone_network_ssid_ = std::nullopt;
  pref_service_->SetInt64(prefs::kPhoneGmsCoreVersion, 0);
  pref_service_->SetInteger(prefs::kPhoneAndroidVersion, 0);
  pref_service_->SetInt64(prefs::kPhoneAmbientApkVersion, 0);
  pref_service_->SetTime(prefs::kPhoneInfoLastUpdatedTime, base::Time());
  pref_service_->SetString(prefs::kPhonePseudonymousId, std::string());
  pref_service_->SetString(prefs::kPhoneManufacturer, std::string());
  pref_service_->SetString(prefs::kPhoneModel, std::string());
  pref_service_->SetString(prefs::kPhoneLocale, std::string());

  network_state_ = NetworkState::kUnknown;
  pref_service_->SetString(prefs::kChromebookPseudonymousId, std::string());
  pref_service_->SetTime(prefs::kPseudonymousIdRotationDate, base::Time());

  ResetSessionId();
}

void PhoneHubStructuredMetricsLogger::ResetSessionId() {
  if (!base::FeatureList::IsEnabled(
          metrics::structured::kPhoneHubStructuredMetrics)) {
    return;
  }
  phone_hub_session_id_ = std::string();
}

void PhoneHubStructuredMetricsLogger::OnNetworkStateListFetched(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {
  for (const auto& network : networks) {
    if (network->type == chromeos::network_config::mojom::NetworkType::kWiFi) {
      std::string hashed_wifi_ssid =
          crypto::SHA256HashString(network->type_state->get_wifi()->ssid);
      if (phone_network_ssid_.has_value() &&
          phone_network_ssid_.value() == hashed_wifi_ssid) {
        network_state_ = NetworkState::kSameNetwork;
      } else {
        network_state_ = NetworkState::kDifferentNetwork;
      }
      return;
    }
  }
  network_state_ = NetworkState::kDifferentNetwork;
}
}  // namespace ash::phonehub

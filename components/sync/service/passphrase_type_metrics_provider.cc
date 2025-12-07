// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/passphrase_type_metrics_provider.h"

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace syncer {

namespace {

using HistogramVersion = PassphraseTypeMetricsProvider::HistogramVersion;

PassphraseTypeForMetrics GetPassphraseTypeForSingleProfile(
    const SyncService& sync_service,
    HistogramVersion histogram_version) {
  bool wait_transport_active = histogram_version == HistogramVersion::kV2;
  if (sync_service.GetTransportState() != SyncService::TransportState::ACTIVE &&
      wait_transport_active) {
    return PassphraseTypeForMetrics::kUnknown;
  }

  const SyncUserSettings* user_settings = sync_service.GetUserSettings();
  CHECK(user_settings);

  std::optional<PassphraseType> passphrase_type =
      user_settings->GetPassphraseType();
  if (!passphrase_type.has_value()) {
    if (histogram_version == HistogramVersion::kV4) {
      // For historical reasons, version 4 of the histogram records "implicit
      // passphrase" if the passphrase type isn't known, which is the case for
      // all signed-out users.
      return PassphraseTypeForMetrics::kImplicitPassphrase;
    } else {
      return PassphraseTypeForMetrics::kUnknown;
    }
  }
  switch (passphrase_type.value()) {
    case PassphraseType::kImplicitPassphrase:
      return PassphraseTypeForMetrics::kImplicitPassphrase;
    case PassphraseType::kKeystorePassphrase:
      return PassphraseTypeForMetrics::kKeystorePassphrase;
    case PassphraseType::kFrozenImplicitPassphrase:
      return PassphraseTypeForMetrics::kFrozenImplicitPassphrase;
    case PassphraseType::kCustomPassphrase:
      return PassphraseTypeForMetrics::kCustomPassphrase;
    case PassphraseType::kTrustedVaultPassphrase:
      return PassphraseTypeForMetrics::kTrustedVaultPassphrase;
  }

  NOTREACHED();
}

PassphraseTypeForMetrics GetPassphraseTypeForAllProfiles(
    const std::vector<const SyncService*>& sync_services,
    PassphraseTypeMetricsProvider::HistogramVersion histogram_version) {
  base::flat_set<std::optional<PassphraseTypeForMetrics>> passphrase_types;
  for (const SyncService* sync_service : sync_services) {
    DCHECK(sync_service);
    passphrase_types.insert(
        GetPassphraseTypeForSingleProfile(*sync_service, histogram_version));
  }

  if (passphrase_types.size() > 1) {
    return PassphraseTypeForMetrics::kInconsistentStateAcrossProfiles;
  }
  if (passphrase_types.empty()) {
    return PassphraseTypeForMetrics::kUnknown;
  }
  return **passphrase_types.begin();
}

}  // namespace

PassphraseTypeMetricsProvider::PassphraseTypeMetricsProvider(
    HistogramVersion histogram_version,
    const GetAllSyncServicesCallback& get_all_sync_services_callback)
    : histogram_version_(histogram_version),
      get_all_sync_services_callback_(get_all_sync_services_callback) {}

PassphraseTypeMetricsProvider::~PassphraseTypeMetricsProvider() = default;

bool PassphraseTypeMetricsProvider::ProvideHistograms() {
  const std::vector<const SyncService*>& sync_services =
      get_all_sync_services_callback_.Run();
  if (sync_services.empty() && (histogram_version_ != HistogramVersion::kV2)) {
    // Record later rather than record kUnknown.
    return false;
  }

  // TODO(crbug.com/347711860): Remove Sync.PassphraseType2 and
  // Sync.PassphraseType4 on 02/2026 once Sync.PassphraseType5 has been
  // available for a year.
  base::UmaHistogramEnumeration(
      GetHistogramName(),
      GetPassphraseTypeForAllProfiles(sync_services, histogram_version_));
  return true;
}

std::string_view PassphraseTypeMetricsProvider::GetHistogramName() const {
  switch (histogram_version_) {
    case HistogramVersion::kV2:
      return "Sync.PassphraseType2";
    case HistogramVersion::kV4:
      return "Sync.PassphraseType4";
    case HistogramVersion::kV5:
      return "Sync.PassphraseType5";
  }
  NOTREACHED();
}

}  // namespace syncer

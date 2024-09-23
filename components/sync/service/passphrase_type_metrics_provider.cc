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

PassphraseTypeForMetrics GetPassphraseTypeForSingleProfile(
    const SyncService& sync_service,
    bool wait_transport_active) {
  if (sync_service.GetTransportState() != SyncService::TransportState::ACTIVE &&
      wait_transport_active) {
    return PassphraseTypeForMetrics::kUnknown;
  }

  const SyncUserSettings* user_settings = sync_service.GetUserSettings();
  CHECK(user_settings);

  // Note: The PassphraseType should always be known here, since the
  // TransportState is active.
  // TODO(crbug.com/40923935): Can this be CHECKed?
  switch (user_settings->GetPassphraseType().value_or(
      PassphraseType::kImplicitPassphrase)) {
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

  NOTREACHED_IN_MIGRATION();
  return PassphraseTypeForMetrics::kUnknown;
}

PassphraseTypeForMetrics GetPassphraseTypeForAllProfiles(
    const std::vector<const SyncService*>& sync_services,
    bool wait_transport_active) {
  base::flat_set<std::optional<PassphraseTypeForMetrics>> passphrase_types;
  for (const SyncService* sync_service : sync_services) {
    DCHECK(sync_service);
    passphrase_types.insert(GetPassphraseTypeForSingleProfile(
        *sync_service, wait_transport_active));
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
    bool use_cached_passphrase_type,
    const GetAllSyncServicesCallback& get_all_sync_services_callback)
    : use_cached_passphrase_type_(use_cached_passphrase_type),
      get_all_sync_services_callback_(get_all_sync_services_callback) {}

PassphraseTypeMetricsProvider::~PassphraseTypeMetricsProvider() = default;

bool PassphraseTypeMetricsProvider::ProvideHistograms() {
  const std::vector<const SyncService*>& sync_services =
      get_all_sync_services_callback_.Run();
  if (sync_services.empty() && use_cached_passphrase_type_) {
    // Record later rather than record kUnknown.
    return false;
  }

  // TODO(crbug.com/347711860): Remove Sync.PassphraseType2 on 06/2025 once
  // Sync.PassphraseType4 has been available for a year.
  base::UmaHistogramEnumeration(
      use_cached_passphrase_type_ ? "Sync.PassphraseType4"
                                  : "Sync.PassphraseType2",
      GetPassphraseTypeForAllProfiles(sync_services,
                                      !use_cached_passphrase_type_));
  return true;
}

}  // namespace syncer

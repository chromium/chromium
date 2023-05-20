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

absl::optional<PassphraseTypeForMetrics> GetPassphraseTypeForSingleProfile(
    const SyncService& sync_service) {
  if (sync_service.GetTransportState() != SyncService::TransportState::ACTIVE) {
    return absl::nullopt;
  }

  const SyncUserSettings* user_settings = sync_service.GetUserSettings();
  // Guaranteed by sync transport state.
  DCHECK(user_settings);

  switch (user_settings->GetPassphraseType()) {
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
  return absl::nullopt;
}

PassphraseTypeForMetrics GetPassphraseTypeForAllProfiles(
    const std::vector<const SyncService*>& sync_services) {
  base::flat_set<absl::optional<PassphraseTypeForMetrics>> passphrase_types;
  for (const SyncService* sync_service : sync_services) {
    DCHECK(sync_service);
    passphrase_types.insert(GetPassphraseTypeForSingleProfile(*sync_service));
  }

  if (passphrase_types.size() > 1) {
    return PassphraseTypeForMetrics::kInconsistentStateAcrossProfiles;
  }
  if (passphrase_types.empty() || !passphrase_types.begin()->has_value()) {
    return PassphraseTypeForMetrics::kNoActiveSyncingProfiles;
  }
  return **passphrase_types.begin();
}

}  // namespace

PassphraseTypeMetricsProvider::PassphraseTypeMetricsProvider(
    const GetAllSyncServicesCallback& get_all_sync_services_callback)
    : get_all_sync_services_callback_(get_all_sync_services_callback) {}

PassphraseTypeMetricsProvider::~PassphraseTypeMetricsProvider() = default;

bool PassphraseTypeMetricsProvider::ProvideHistograms() {
  base::UmaHistogramEnumeration(
      "Sync.PassphraseType2",
      GetPassphraseTypeForAllProfiles(get_all_sync_services_callback_.Run()));
  return true;
}

}  // namespace syncer

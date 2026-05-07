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
    const SyncService& sync_service) {
  const SyncUserSettings* user_settings = sync_service.GetUserSettings();
  CHECK(user_settings);

  std::optional<PassphraseType> passphrase_type =
      user_settings->GetPassphraseType();
  if (!passphrase_type.has_value()) {
    return PassphraseTypeForMetrics::kUnknown;
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
    const std::vector<const SyncService*>& sync_services) {
  base::flat_set<std::optional<PassphraseTypeForMetrics>> passphrase_types;
  for (const SyncService* sync_service : sync_services) {
    DCHECK(sync_service);
    passphrase_types.insert(GetPassphraseTypeForSingleProfile(*sync_service));
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
    const GetAllSyncServicesCallback& get_all_sync_services_callback)
    : get_all_sync_services_callback_(get_all_sync_services_callback) {}

PassphraseTypeMetricsProvider::~PassphraseTypeMetricsProvider() = default;

bool PassphraseTypeMetricsProvider::ProvideHistograms() {
  const std::vector<const SyncService*>& sync_services =
      get_all_sync_services_callback_.Run();
  if (sync_services.empty()) {
    // Record later rather than record kUnknown.
    return false;
  }

  base::UmaHistogramEnumeration("Sync.PassphraseType5",
                                GetPassphraseTypeForAllProfiles(sync_services));
  return true;
}

}  // namespace syncer

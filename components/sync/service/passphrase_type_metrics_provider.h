// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_PASSPHRASE_TYPE_METRICS_PROVIDER_H_
#define COMPONENTS_SYNC_SERVICE_PASSPHRASE_TYPE_METRICS_PROVIDER_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/metrics/metrics_provider.h"

namespace syncer {

class SyncService;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with the homonym enum
// in tools/metrics/histograms/metadata/sync/enums.xml.
// Exposed in the header file for testing.
// LINT.IfChange(PassphraseTypeForMetrics)
enum class PassphraseTypeForMetrics {
  // Passphrase type is unknown.
  kUnknown = 0,
  // Used if there are multiple syncing profiles with different passphrase
  // types or with different sync transport state is ACTIVE values.
  kInconsistentStateAcrossProfiles = 1,
  // Further values correspond to regular PassphraseType. Used if there is only
  // one syncing profile or all profiles have the same PassphraseType.
  kImplicitPassphrase = 2,
  kKeystorePassphrase = 3,
  kFrozenImplicitPassphrase = 4,
  kCustomPassphrase = 5,
  kTrustedVaultPassphrase = 6,
  kMaxValue = kTrustedVaultPassphrase
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:PassphraseTypeForMetrics)

// A registerable metrics provider that will emit sync passphrase type upon UMA
// upload. If it's impossible to detect real passphrase type, special enum
// values will be used (kNoActiveSyncingProfiles and
// kVariousStateAcrossProfiles).
class PassphraseTypeMetricsProvider : public metrics::MetricsProvider {
 public:
  using GetAllSyncServicesCallback =
      base::RepeatingCallback<std::vector<const SyncService*>()>;

  // All SyncServices returned by |get_all_sync_services_callback| must be not
  // null. If `use_cached_passphrase_type` is true, the metric will be computed
  // based on the passphrase type that's cached in sync prefs.
  PassphraseTypeMetricsProvider(
      bool use_cached_passphrase_type,
      const GetAllSyncServicesCallback& get_all_sync_services_callback);

  PassphraseTypeMetricsProvider(const PassphraseTypeMetricsProvider& other) =
      delete;
  PassphraseTypeMetricsProvider& operator=(
      const PassphraseTypeMetricsProvider& other) = delete;

  ~PassphraseTypeMetricsProvider() override;

  // MetricsProvider overrides.
  bool ProvideHistograms() override;

 private:
  const bool use_cached_passphrase_type_;
  const GetAllSyncServicesCallback get_all_sync_services_callback_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_PASSPHRASE_TYPE_METRICS_PROVIDER_H_

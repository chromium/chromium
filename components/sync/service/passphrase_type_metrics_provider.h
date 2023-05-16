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

// Used for UMA. Exposed in the header file for testing.
enum class PassphraseTypeForMetrics {
  // Used if there are no syncing profiles or all syncing profiles are not in
  // ACTIVE sync transport state.
  kNoActiveSyncingProfiles,
  // Used if there are multiple syncing profiles with different passphrase
  // types or with different sync transport state is ACTIVE values.
  kInconsistentStateAcrossProfiles,
  // Further values correspond to regular PassphraseType. Used if there is only
  // one syncing profile or all profiles have the same PassphraseType.
  kImplicitPassphrase,
  kKeystorePassphrase,
  kFrozenImplicitPassphrase,
  kCustomPassphrase,
  kTrustedVaultPassphrase,
  kMaxValue = kTrustedVaultPassphrase
};

// A registerable metrics provider that will emit sync passphrase type upon UMA
// upload. If it's impossible to detect real passphrase type, special enum
// values will be used (kNoActiveSyncingProfiles and
// kVariousStateAcrossProfiles).
class PassphraseTypeMetricsProvider : public metrics::MetricsProvider {
 public:
  using GetAllSyncServicesCallback =
      base::RepeatingCallback<std::vector<const SyncService*>()>;

  // All SyncServices returned by |get_all_sync_services_callback| must be not
  // null.
  explicit PassphraseTypeMetricsProvider(
      const GetAllSyncServicesCallback& get_all_sync_services_callback);

  PassphraseTypeMetricsProvider(const PassphraseTypeMetricsProvider& other) =
      delete;
  PassphraseTypeMetricsProvider& operator=(
      const PassphraseTypeMetricsProvider& other) = delete;

  ~PassphraseTypeMetricsProvider() override;

  // MetricsProvider overrides.
  bool ProvideHistograms() override;

 private:
  const GetAllSyncServicesCallback get_all_sync_services_callback_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_PASSPHRASE_TYPE_METRICS_PROVIDER_H_

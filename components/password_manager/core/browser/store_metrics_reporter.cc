// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/store_metrics_reporter.h"

#include "base/metrics/histogram_functions.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"

namespace password_manager {

StoreMetricsReporter::StoreMetricsReporter(
    PasswordManagerClient* client,
    const syncer::SyncService* sync_service,
    const signin::IdentityManager* identity_manager,
    PrefService* prefs) {
  // May be null in tests.
  if (PasswordStore* store = client->GetProfilePasswordStore()) {
    store->ReportMetrics(
        password_manager::sync_util::GetSyncUsernameIfSyncingPasswords(
            sync_service, identity_manager),
        client->GetPasswordSyncState() ==
            password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE,
        client->IsUnderAdvancedProtection());
  }
  base::UmaHistogramBoolean(
      "PasswordManager.Enabled",
      prefs->GetBoolean(password_manager::prefs::kCredentialsEnableService));
  base::UmaHistogramBoolean(
      "PasswordManager.LeakDetection.Enabled",
      prefs->GetBoolean(
          password_manager::prefs::kPasswordLeakDetectionEnabled));
  password_manager::metrics_util::LogOnboardingState(
      static_cast<password_manager::metrics_util::OnboardingState>(
          prefs->GetInteger(
              password_manager::prefs::kPasswordManagerOnboardingState)));
}

StoreMetricsReporter::~StoreMetricsReporter() = default;

}  // namespace password_manager

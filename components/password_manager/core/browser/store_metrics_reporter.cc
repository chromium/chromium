// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/store_metrics_reporter.h"

#include "base/metrics/histogram_macros.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_sync_util.h"

namespace password_manager {

StoreMetricsReporter::StoreMetricsReporter(
    bool password_manager_enabled,
    PasswordManagerClient* client,
    const syncer::SyncService* sync_service,
    const SigninManagerBase* signin_manager,
    PrefService* prefs) {
  password_manager::PasswordStore* store = client->GetPasswordStore();
  // May be null in tests.
  if (store) {
    store->ReportMetrics(
        password_manager::sync_util::GetSyncUsernameIfSyncingPasswords(
            sync_service, signin_manager),
        client->GetPasswordSyncState() ==
            password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE,
        client->IsUnderAdvancedProtection());
  }
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.Enabled", password_manager_enabled);
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.ShouldShowAutoSignInFirstRunExperience",
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          prefs));
}

StoreMetricsReporter::~StoreMetricsReporter() = default;

}  // namespace password_manager

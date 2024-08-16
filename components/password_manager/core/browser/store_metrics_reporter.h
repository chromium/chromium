// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_METRICS_REPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_METRICS_REPORTER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_store/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

class PrefService;

namespace syncer {
class SyncService;
}  //  namespace syncer

namespace password_manager {

class PasswordManagerSettingsService;
class PasswordReuseManager;

// Instantiate this object to report metrics about the contents of the password
// store.
class StoreMetricsReporter : public PasswordStoreConsumer {
 public:
  // Used for counting credentials that were found in both stores.
  struct CredentialsCount {
    int profile_credentials_count = 0;
    int account_credentials_count = 0;
  };

  // Reports various metrics based on whether password manager is enabled. Uses
  // |sync_service| password syncing state. Uses |sync_service| to obtain the
  // sync username to report about its presence among saved credentials. Uses
  // the |prefs| to obtain information whether the password manager and the leak
  // detection feature is enabled. |done_call| is run after all metrics
  // reporting is done from the store.
  StoreMetricsReporter(PasswordStoreInterface* profile_store,
                       PasswordStoreInterface* account_store,
                       const syncer::SyncService* sync_service,
                       PrefService* prefs,
                       PasswordReuseManager* password_reuse_manager,
                       PasswordManagerSettingsService* settings,
                       base::OnceClosure done_callback);
  StoreMetricsReporter(const StoreMetricsReporter&) = delete;
  StoreMetricsReporter& operator=(const StoreMetricsReporter&) = delete;
  StoreMetricsReporter(StoreMetricsReporter&&) = delete;
  StoreMetricsReporter& operator=(StoreMetricsReporter&&) = delete;
  ~StoreMetricsReporter() override;

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;
  void OnGetPasswordStoreResultsFrom(
      PasswordStoreInterface* store,
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  void OnBackgroundMetricsReportingCompleted(
      CredentialsCount credentials_count);

  // Since metrics reporting is run in a delayed task, we grab refptrs to the
  // stores, to ensure they're still alive when the delayed task runs.
  scoped_refptr<PasswordStoreInterface> profile_store_;
  scoped_refptr<PasswordStoreInterface> account_store_;

  raw_ptr<PrefService> prefs_ = nullptr;

  std::string sync_username_;

  bool custom_passphrase_enabled_;

  bool is_opted_in_account_storage_;

  bool is_safe_browsing_enabled_;

  // Temporarily holds the credentials stored in the profile and account stores
  // till the actual metric computation starts. They don't have a value until
  // the credentials are loaded from the storage.
  std::optional<std::vector<std::unique_ptr<PasswordForm>>>
      profile_store_results_;
  std::optional<std::vector<std::unique_ptr<PasswordForm>>>
      account_store_results_;

  base::OnceClosure done_callback_;
  base::WeakPtrFactory<StoreMetricsReporter> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_METRICS_REPORTER_H_

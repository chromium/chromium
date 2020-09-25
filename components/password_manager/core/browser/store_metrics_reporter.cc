// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/store_metrics_reporter.h"

#include "base/metrics/histogram_functions.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"

namespace password_manager {

namespace {

class Consumer : public PasswordStoreConsumer {
 public:
  explicit Consumer(
      PasswordStore* store,
      base::OnceCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>
          results_callback)
      : results_callback_(std::move(results_callback)) {
    store->GetAutofillableLogins(this);
  }
  ~Consumer() override = default;

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    std::move(results_callback_).Run(std::move(results));
    // Note: |this| might be destroyed now.
  }

 private:
  base::OnceCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>
      results_callback_;
};

}  // namespace

class StoreMetricsReporter::MultiStoreMetricsReporter {
 public:
  MultiStoreMetricsReporter(PasswordStore* profile_store,
                            PasswordStore* account_store,
                            base::OnceClosure done_callback)
      : done_callback_(std::move(done_callback)) {
    DCHECK(profile_store);
    DCHECK(account_store);
    profile_store_consumer_ = std::make_unique<Consumer>(
        profile_store,
        base::BindOnce(&MultiStoreMetricsReporter::ReceiveResults,
                       base::Unretained(this), /*is_account_store=*/false));
    account_store_consumer_ = std::make_unique<Consumer>(
        account_store,
        base::BindOnce(&MultiStoreMetricsReporter::ReceiveResults,
                       base::Unretained(this), /*is_account_store=*/true));
  }

 private:
  void ReceiveResults(bool is_account_store,
                      std::vector<std::unique_ptr<PasswordForm>> results) {
    if (is_account_store) {
      for (const std::unique_ptr<PasswordForm>& form : results) {
        account_store_results_.insert(std::make_pair(
            std::make_pair(form->signon_realm, form->username_value),
            form->password_value));
      }
      account_store_consumer_.reset();
    } else {
      for (const std::unique_ptr<PasswordForm>& form : results) {
        profile_store_results_.insert(std::make_pair(
            std::make_pair(form->signon_realm, form->username_value),
            form->password_value));
      }
      profile_store_consumer_.reset();
    }

    if (!profile_store_consumer_ && !account_store_consumer_)
      ReportMetrics();
  }

  void ReportMetrics() {
    // Count the contents of the account store as compared to the profile store:
    // - Additional:  Credentials that are in the account store, but not in the
    //                profile store.
    // - Missing:     Credentials that are in the profile store, but not in the
    //                account store.
    // - Identical:   Credentials that are in both stores.
    // - Conflicting: Credentials with the same signon realm and username, but
    //                different passwords in the two stores.
    int additional = 0;
    int missing = 0;
    int identical = 0;
    int conflicting = 0;

    // Go over the data from both stores in parallel, always advancing in the
    // one that is "behind". The entries are sorted by signon_realm and
    // username (the exact ordering doesn't matter, just that it's consistent).
    auto profile_it = profile_store_results_.begin();
    auto account_it = account_store_results_.begin();
    while (account_it != account_store_results_.end()) {
      // First, go over any entries in the profile store that don't exist in the
      // account store.
      while (profile_it != profile_store_results_.end() &&
             profile_it->first < account_it->first) {
        ++missing;
        ++profile_it;
      }
      // Now profile_it->first is >= account_it->first.
      // Check if they match.
      if (profile_it != profile_store_results_.end() &&
          account_it->first == profile_it->first) {
        // The signon_realm and username match, check the password value.
        if (account_it->second == profile_it->second)
          ++identical;
        else
          ++conflicting;

        ++profile_it;
      } else {
        // The signon_realm and username don't match, so this is an account
        // store entry that doesn't exist in the profile store.
        ++additional;
      }

      ++account_it;
    }
    // We're done with the account store. Go over any remaining profile store
    // entries.
    while (profile_it != profile_store_results_.end()) {
      ++missing;
      ++profile_it;
    }

    base::UmaHistogramCounts100(
        "PasswordManager.AccountStoreVsProfileStore.Additional", additional);
    base::UmaHistogramCounts100(
        "PasswordManager.AccountStoreVsProfileStore.Missing", missing);
    base::UmaHistogramCounts100(
        "PasswordManager.AccountStoreVsProfileStore.Identical", identical);
    base::UmaHistogramCounts100(
        "PasswordManager.AccountStoreVsProfileStore.Conflicting", conflicting);

    std::move(done_callback_).Run();
    // Note: |this| might be destroyed now.
  }

  base::OnceClosure done_callback_;

  std::unique_ptr<Consumer> profile_store_consumer_;
  std::unique_ptr<Consumer> account_store_consumer_;

  // Maps from (signon_realm, username) to password.
  std::map<std::pair<std::string, base::string16>, base::string16>
      profile_store_results_;
  std::map<std::pair<std::string, base::string16>, base::string16>
      account_store_results_;
};

StoreMetricsReporter::StoreMetricsReporter(
    PasswordManagerClient* client,
    const syncer::SyncService* sync_service,
    const signin::IdentityManager* identity_manager,
    PrefService* prefs) {
  for (PasswordStore* store :
       {client->GetProfilePasswordStore(), client->GetAccountPasswordStore()}) {
    // May be null in tests. The account store is also null if the
    // kEnablePasswordsAccountStorage feature is disabled.
    if (store) {
      store->ReportMetrics(
          password_manager::sync_util::GetSyncUsernameIfSyncingPasswords(
              sync_service, identity_manager),
          client->GetPasswordSyncState() ==
              password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE,
          client->IsUnderAdvancedProtection());
    }
  }
  base::UmaHistogramBoolean(
      "PasswordManager.Enabled",
      prefs->GetBoolean(password_manager::prefs::kCredentialsEnableService));
  base::UmaHistogramBoolean(
      "PasswordManager.LeakDetection.Enabled",
      prefs->GetBoolean(
          password_manager::prefs::kPasswordLeakDetectionEnabled));

  // If both stores exist, kick off the MultiStoreMetricsReporter.
  PasswordStore* profile_store = client->GetProfilePasswordStore();
  PasswordStore* account_store = client->GetAccountPasswordStore();
  if (profile_store && account_store) {
    // Delay the actual reporting by 30 seconds, to ensure it doesn't happen
    // during the "hot phase" of Chrome startup. (This is what
    // PasswordStore::ReportMetrics also does.)
    // Grab refptrs to the stores, to ensure they're still alive when the
    // delayed task runs.
    scoped_refptr<PasswordStore> retained_profile_store = profile_store;
    scoped_refptr<PasswordStore> retained_account_store = account_store;
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&StoreMetricsReporter::ReportMultiStoreMetrics,
                       weak_ptr_factory_.GetWeakPtr(), retained_profile_store,
                       retained_account_store),
        base::TimeDelta::FromSeconds(30));
  }
}

void StoreMetricsReporter::ReportMultiStoreMetrics(
    scoped_refptr<PasswordStore> profile_store,
    scoped_refptr<PasswordStore> account_store) {
  multi_store_reporter_ = std::make_unique<MultiStoreMetricsReporter>(
      profile_store.get(), account_store.get(),
      base::BindOnce(&StoreMetricsReporter::MultiStoreMetricsDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void StoreMetricsReporter::MultiStoreMetricsDone() {
  multi_store_reporter_.reset();
}

StoreMetricsReporter::~StoreMetricsReporter() = default;

}  // namespace password_manager

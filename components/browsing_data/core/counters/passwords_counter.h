// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_COUNTERS_PASSWORDS_COUNTER_H_
#define COMPONENTS_BROWSING_DATA_CORE_COUNTERS_PASSWORDS_COUNTER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/sync_tracker.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace browsing_data {

class PasswordsCounter : public browsing_data::BrowsingDataCounter,
                         public password_manager::PasswordStoreConsumer,
                         public password_manager::PasswordStore::Observer {
 public:
  // A subclass of SyncResult that stores the result value, a boolean
  // representing whether the datatype is synced, and a vector of example
  // domains where credentials would be deleted.
  class PasswordsResult : public SyncResult {
   public:
    PasswordsResult(const BrowsingDataCounter* source,
                    ResultInt value,
                    bool sync_enabled,
                    std::vector<std::string> domain_examples);
    ~PasswordsResult() override;

    const std::vector<std::string>& domain_examples() const {
      return domain_examples_;
    }

   private:
    std::vector<std::string> domain_examples_;

    DISALLOW_COPY_AND_ASSIGN(PasswordsResult);
  };

  explicit PasswordsCounter(
      scoped_refptr<password_manager::PasswordStore> store,
      syncer::SyncService* sync_service);
  ~PasswordsCounter() override;

  const char* GetPrefName() const override;

 protected:
  virtual std::unique_ptr<PasswordsResult> MakeResult();

  bool is_sync_active() { return sync_tracker_.IsSyncActive(); }
  int num_passwords() { return num_passwords_; }
  const std::vector<std::string>& domain_examples() { return domain_examples_; }

 private:
  void OnInitialized() override;

  // Counting is done asynchronously in a request to PasswordStore.
  // This callback returns the results, which are subsequently reported.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // Called when the contents of the password store change. Triggers new
  // counting.
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  void Count() override;

  base::CancelableTaskTracker cancelable_task_tracker_;
  scoped_refptr<password_manager::PasswordStore> store_;
  SyncTracker sync_tracker_;
  int num_passwords_;
  std::vector<std::string> domain_examples_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COUNTERS_PASSWORDS_COUNTER_H_

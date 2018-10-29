// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_COUNTERS_PASSWORDS_COUNTER_H_
#define COMPONENTS_BROWSING_DATA_CORE_COUNTERS_PASSWORDS_COUNTER_H_

#include <memory>
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
  explicit PasswordsCounter(
      scoped_refptr<password_manager::PasswordStore> store,
      syncer::SyncService* sync_service);
  ~PasswordsCounter() override;

  const char* GetPrefName() const override;

 protected:
  virtual std::unique_ptr<SyncResult> MakeResult();

  bool is_sync_active() { return sync_tracker_.IsSyncActive(); };
  int num_passwords() { return num_passwords_; }

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
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COUNTERS_PASSWORDS_COUNTER_H_

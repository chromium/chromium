// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_METRICS_REPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_METRICS_REPORTER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/supports_user_data.h"
#include "components/password_manager/core/browser/password_store.h"

class PrefService;

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}  //  namespace syncer

namespace password_manager {

class PasswordReuseManager;

// Instantiate this object to report metrics about the contents of the password
// store.
class StoreMetricsReporter {
 public:
  // Reports various metrics based on whether password manager is enabled. Uses
  // |sync_service| password syncing state. Uses |sync_service| and
  // |identity_manager| to obtain the sync username to report about its presence
  // among saved credentials. Uses the |prefs| to obtain information whether the
  // password manager and the leak detection feature is enabled.
  StoreMetricsReporter(PasswordStore* profile_store,
                       PasswordStore* account_store,
                       const syncer::SyncService* sync_service,
                       const signin::IdentityManager* identity_manager,
                       PrefService* prefs,
                       PasswordReuseManager* password_reuse_manager,
                       bool is_under_advanced_protection);

  ~StoreMetricsReporter();

 private:
  class MultiStoreMetricsReporter;

  void ReportMultiStoreMetrics(scoped_refptr<PasswordStore> profile_store,
                               scoped_refptr<PasswordStore> account_store,
                               bool is_opted_in);
  void MultiStoreMetricsDone();

  // Since metrics reporting is run in a delayed task, we grab refptrs to the
  // stores, to ensure they're still alive when the delayed task runs.
  scoped_refptr<PasswordStore> profile_store_;
  scoped_refptr<PasswordStore> account_store_;

  std::unique_ptr<MultiStoreMetricsReporter> multi_store_reporter_;

  base::WeakPtrFactory<StoreMetricsReporter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StoreMetricsReporter);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_METRICS_REPORTER_H_

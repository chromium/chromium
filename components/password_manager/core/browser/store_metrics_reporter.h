// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_METRICS_REPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_METRICS_REPORTER_H_

#include "base/macros.h"

class PrefService;

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}  //  namespace syncer

namespace password_manager {

class PasswordManagerClient;

// Instantiate this object to report metrics about the contents of the password
// store. Create a static base::NoDestructor<StoreMetricsReporter> to ensure
// that metrics are reported only once per process. This is thread-safe, because
// C++11 guarantees that: "If control enters the declaration concurrently while
// the variable is being initialized, the concurrent execution shall wait for
// completion of the initialization." So the reporter is only initialised (and
// hence reports) once.
class StoreMetricsReporter {
 public:
  // Reports various metrics based on whether password manager is enabled. Uses
  // |client| to obtain the password store and password syncing state. Uses
  // |sync_service| and |identity_manager| to obtain the sync username to report
  // about its presence among saved credentials. Uses the |prefs| to obtain
  // information wither the password manager and the leak detection feature is
  // enabled.
  StoreMetricsReporter(PasswordManagerClient* client,
                       const syncer::SyncService* sync_service,
                       const signin::IdentityManager* identity_manager,
                       PrefService* prefs);

  ~StoreMetricsReporter();

 private:
  DISALLOW_COPY_AND_ASSIGN(StoreMetricsReporter);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STORE_METRICS_REPORTER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_FAKE_SYNC_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_FAKE_SYNC_SERVICE_PROVIDER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/sync/sync_service_provider.h"
#include "components/account_id/account_id.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ash {

// Test SyncServiceProvider that maps account ids to SyncService instances.
// Installs itself as the process-wide provider on construction. Find() returns
// a service only for accounts explicitly registered via
// SetSyncServiceForAccount (and nullptr otherwise), so a service is never
// handed back for a user that a test has not set up as signed-in.
class FakeSyncServiceProvider : public SyncServiceProvider {
 public:
  FakeSyncServiceProvider();
  ~FakeSyncServiceProvider() override;

  // Registers `sync_service` for `account_id`. Passing nullptr clears the
  // association.
  void SetSyncServiceForAccount(const AccountId& account_id,
                                syncer::SyncService* sync_service);

  // SyncServiceProvider:
  syncer::SyncService* Find(const AccountId& account_id) override;

 private:
  std::map<AccountId, raw_ptr<syncer::SyncService>> sync_services_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_FAKE_SYNC_SERVICE_PROVIDER_H_

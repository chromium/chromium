// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DESKS_STORAGE_DESK_SYNC_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_DESKS_STORAGE_DESK_SYNC_SERVICE_PROVIDER_H_

class AccountId;

namespace desks_storage {
class DeskSyncService;
}  // namespace desks_storage

namespace ash {

// Provides the desks_storage::DeskSyncService associated with a user to
// ChromeOS callers without forcing them to depend on //chrome/browser/sync's
// Profile-keyed factory. The concrete implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// desk_sync_service_provider_impl.h).
class DeskSyncServiceProvider {
 public:
  DeskSyncServiceProvider();
  DeskSyncServiceProvider(const DeskSyncServiceProvider&) = delete;
  DeskSyncServiceProvider& operator=(const DeskSyncServiceProvider&) = delete;
  virtual ~DeskSyncServiceProvider();

  // Returns the process-wide singleton.
  static DeskSyncServiceProvider& Get();

  // Returns the DeskSyncService associated with `account_id`, or nullptr if
  // none is available. The returned pointer is owned by the
  // BrowserContext-keyed service infrastructure; callers must not delete it.
  virtual desks_storage::DeskSyncService* Find(const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DESKS_STORAGE_DESK_SYNC_SERVICE_PROVIDER_H_

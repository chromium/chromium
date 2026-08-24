// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_SYNC_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_SYNC_SERVICE_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ash {

// Provides the syncer::SyncService associated with a user to ChromeOS callers
// without forcing them to depend on //chrome/browser/sync's Profile-keyed
// factory. The concrete implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// sync_service_provider_impl.h).
class COMPONENT_EXPORT(SYNC_SERVICE_PROVIDER) SyncServiceProvider {
 public:
  SyncServiceProvider();
  SyncServiceProvider(const SyncServiceProvider&) = delete;
  SyncServiceProvider& operator=(const SyncServiceProvider&) = delete;
  virtual ~SyncServiceProvider();

  // Returns the process-wide singleton.
  static SyncServiceProvider& Get();

  // Returns the SyncService associated with `account_id`, or nullptr if none is
  // available. The returned pointer is owned by the BrowserContext-keyed
  // service infrastructure; callers must not delete it.
  virtual syncer::SyncService* Find(const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_SYNC_SERVICE_PROVIDER_H_

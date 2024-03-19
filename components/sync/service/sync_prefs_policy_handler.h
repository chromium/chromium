// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_PREFS_POLICY_HANDLER_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_PREFS_POLICY_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {

// SyncPrefsPolicyHandler applies appropriate changes to Sync Prefs when the
// SyncDisabled policy, or SyncTypesListDisabled policy is applied.
// Note: There's another class, SyncPolicyHandler, which sets policy-controlled
// values of the prefs (as is usual for policies). This class updates the actual
// user-controlled values of the prefs, so that, if the policy gets lifted, sync
// or its data types don't suddenly become active.
class SyncPrefsPolicyHandler : public SyncServiceObserver {
 public:
  explicit SyncPrefsPolicyHandler(SyncService* sync_service);

  SyncPrefsPolicyHandler(const SyncPrefsPolicyHandler&) = delete;
  SyncPrefsPolicyHandler& operator=(const SyncPrefsPolicyHandler&) = delete;

  ~SyncPrefsPolicyHandler() override;

  // SyncServiceObserver:
  void OnStateChanged(SyncService* sync_service) override;
  void OnSyncShutdown(SyncService* sync_service) override;

 private:
  // Disables the data types that are currently disabled by policy; SyncDisabled
  // or SyncTypesListDisabled, so these types become disabled when the policy
  // gets lifted.
  void EnforcePolicyOnDataTypes();

  raw_ptr<SyncService> sync_service_ = nullptr;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_PREFS_POLICY_HANDLER_H_

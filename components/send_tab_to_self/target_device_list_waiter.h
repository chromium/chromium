// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_LIST_WAITER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_LIST_WAITER_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace send_tab_to_self {

enum class EntryPointDisplayReason;

// Shared utility to wait for Sync to download the target device list (or
// determine that no target devices exist) after sign-in.
class TargetDeviceListWaiter : public syncer::SyncServiceObserver {
 public:
  using GetDisplayReasonCallback = base::RepeatingCallback<
      std::optional<send_tab_to_self::EntryPointDisplayReason>()>;

  // Queries `get_display_reason_callback` until it indicates the device list is
  // known (i.e. until it returns kOfferFeature or kInformNoTargetDevice), then
  // calls `on_list_known_callback`. The callback may run synchronously inside
  // this constructor if the display reason is already known. Destroying the
  // object aborts the waiting.
  TargetDeviceListWaiter(
      syncer::SyncService* sync_service,
      const GetDisplayReasonCallback& get_display_reason_callback,
      base::OnceClosure on_list_known_callback);

  TargetDeviceListWaiter(const TargetDeviceListWaiter&) = delete;
  TargetDeviceListWaiter& operator=(const TargetDeviceListWaiter&) = delete;

  ~TargetDeviceListWaiter() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

 private:
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  GetDisplayReasonCallback get_display_reason_callback_;
  base::OnceClosure on_list_known_callback_;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_LIST_WAITER_H_

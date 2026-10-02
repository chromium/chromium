// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_LIST_WAITER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_LIST_WAITER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "components/sync/service/sync_service_observer.h"
#include "url/gurl.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace send_tab_to_self {

class SendTabToSelfModel;
class SendTabToSelfSyncService;

// Shared utility to wait until the target device list is known (or it is
// determined that no target devices exist), e.g. after sign-in while Sync
// downloads the device list, or while the model and DeviceInfo are still
// loading from disk.
class TargetDeviceListWaiter : public syncer::SyncServiceObserver,
                               public SendTabToSelfModelObserver {
 public:
  // Queries `send_tab_to_self_service` until it indicates the device list is
  // known (i.e. until it returns kOfferFeature or kInformNoTargetDevice), then
  // asynchronously calls `on_list_known_callback`. The query is re-run on every
  // SyncService state change and whenever the SendTabToSelfModel notifies
  // OnModelReady(). Destroying the object aborts the waiting and cancels
  // callback execution.
  // `sync_service`, `send_tab_to_self_service`, and `on_list_known_callback`
  // must all be non-null.
  TargetDeviceListWaiter(
      syncer::SyncService* sync_service,
      SendTabToSelfSyncService* send_tab_to_self_service,
      const GURL& url_to_share,
      base::OnceClosure on_list_known_callback);

  TargetDeviceListWaiter(const TargetDeviceListWaiter&) = delete;
  TargetDeviceListWaiter& operator=(const TargetDeviceListWaiter&) = delete;

  ~TargetDeviceListWaiter() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

  // SendTabToSelfModelObserver:
  void OnModelReady() override;

 private:
  void RunCallback();

  raw_ptr<SendTabToSelfSyncService> send_tab_to_self_service_;
  const GURL url_to_share_;
  base::OnceClosure on_list_known_callback_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  base::ScopedObservation<SendTabToSelfModel, SendTabToSelfModelObserver>
      model_observation_{this};

  base::WeakPtrFactory<TargetDeviceListWaiter> weak_ptr_factory_{this};
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_LIST_WAITER_H_

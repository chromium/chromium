// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/target_device_list_waiter.h"

#include <utility>

#include "base/check.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/service/sync_service.h"

namespace send_tab_to_self {

TargetDeviceListWaiter::TargetDeviceListWaiter(
    syncer::SyncService* sync_service,
    SendTabToSelfSyncService* send_tab_to_self_service,
    const GURL& url_to_share,
    base::OnceClosure on_list_known_callback)
    : send_tab_to_self_service_(send_tab_to_self_service),
      url_to_share_(url_to_share),
      on_list_known_callback_(std::move(on_list_known_callback)) {
  CHECK(on_list_known_callback_);
  if (sync_service) {
    sync_observation_.Observe(sync_service);
  }
  OnStateChanged(nullptr);
}

TargetDeviceListWaiter::~TargetDeviceListWaiter() = default;

void TargetDeviceListWaiter::OnStateChanged(syncer::SyncService* /*sync_service*/) {
  if (!on_list_known_callback_ || !send_tab_to_self_service_) {
    return;
  }
  std::optional<EntryPointDisplayReason> display_reason =
      send_tab_to_self_service_->GetEntryPointDisplayReason(url_to_share_);
  if (!display_reason) {
    return;
  }
  switch (*display_reason) {
    case EntryPointDisplayReason::kOfferSignIn:
    case EntryPointDisplayReason::kOfferReauth:
      break;
    case EntryPointDisplayReason::kOfferFeature:
    case EntryPointDisplayReason::kInformNoTargetDevice:
      sync_observation_.Reset();
      std::move(on_list_known_callback_).Run();
      break;
  }
}

void TargetDeviceListWaiter::OnSyncShutdown(
    syncer::SyncService* /*sync_service*/) {
  // `send_tab_to_self_service_` is guaranteed to be valid during SyncService
  // shutdown because SyncServiceFactory depends on
  // SendTabToSelfSyncServiceFactory.
  sync_observation_.Reset();
  on_list_known_callback_.Reset();
  send_tab_to_self_service_ = nullptr;
}

}  // namespace send_tab_to_self

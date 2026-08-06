// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/target_device_list_waiter.h"

#include <utility>

#include "base/check.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/sync/service/sync_service.h"

namespace send_tab_to_self {

TargetDeviceListWaiter::TargetDeviceListWaiter(
    syncer::SyncService* sync_service,
    const GetDisplayReasonCallback& get_display_reason_callback,
    base::OnceClosure on_list_known_callback)
    : get_display_reason_callback_(get_display_reason_callback),
      on_list_known_callback_(std::move(on_list_known_callback)) {
  CHECK(get_display_reason_callback_);
  CHECK(on_list_known_callback_);
  if (sync_service) {
    sync_observation_.Observe(sync_service);
  }
  OnStateChanged(nullptr);
}

TargetDeviceListWaiter::~TargetDeviceListWaiter() = default;

void TargetDeviceListWaiter::OnStateChanged(syncer::SyncService* /*sync_service*/) {
  if (!on_list_known_callback_) {
    return;
  }
  std::optional<EntryPointDisplayReason> display_reason =
      get_display_reason_callback_.Run();
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

void TargetDeviceListWaiter::OnSyncShutdown(syncer::SyncService* /*sync_service*/) {
  sync_observation_.Reset();
  on_list_known_callback_.Reset();
}

}  // namespace send_tab_to_self

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"

#include <utility>

#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/sync/base/data_type.h"
#include "url/gurl.h"

namespace send_tab_to_self {

StubSendTabToSelfSyncService::StubSendTabToSelfSyncService()
    : fake_delegate_(syncer::SEND_TAB_TO_SELF),
      entry_point_display_reason_(EntryPointDisplayReason::kOfferFeature) {}

StubSendTabToSelfSyncService::~StubSendTabToSelfSyncService() = default;

std::optional<EntryPointDisplayReason>
StubSendTabToSelfSyncService::GetEntryPointDisplayReason(
    const GURL& url_to_share) {
  return entry_point_display_reason_;
}

void StubSendTabToSelfSyncService::SetEntryPointDisplayReason(
    std::optional<EntryPointDisplayReason> reason) {
  entry_point_display_reason_ = reason;
}

SendTabToSelfModel* StubSendTabToSelfSyncService::GetSendTabToSelfModel() {
  return &model_;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
StubSendTabToSelfSyncService::GetControllerDelegate() {
  return fake_delegate_.GetWeakPtr();
}

FakeSendTabToSelfModel*
StubSendTabToSelfSyncService::GetFakeSendTabToSelfModel() {
  return &model_;
}

}  // namespace send_tab_to_self

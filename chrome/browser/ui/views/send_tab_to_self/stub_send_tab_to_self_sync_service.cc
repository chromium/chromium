// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/stub_send_tab_to_self_sync_service.h"

#include "components/keyed_service/core/keyed_service.h"

namespace send_tab_to_self {

StubSendTabToSelfSyncService::StubSendTabToSelfSyncService()
    : fake_delegate_(syncer::SEND_TAB_TO_SELF) {}

StubSendTabToSelfSyncService::~StubSendTabToSelfSyncService() = default;

SendTabToSelfModel* StubSendTabToSelfSyncService::GetSendTabToSelfModel() {
  return &model_fake_;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
StubSendTabToSelfSyncService::GetControllerDelegate() {
  return fake_delegate_.GetWeakPtr();
}

std::optional<EntryPointDisplayReason>
StubSendTabToSelfSyncService::GetEntryPointDisplayReason(
    const GURL& url_to_share) {
  return EntryPointDisplayReason::kOfferFeature;
}

FakeSendTabToSelfModel* StubSendTabToSelfSyncService::GetModelFake() {
  return &model_fake_;
}

std::unique_ptr<KeyedService> BuildStubSyncService(
    content::BrowserContext* context) {
  return std::make_unique<StubSendTabToSelfSyncService>();
}

}  // namespace send_tab_to_self

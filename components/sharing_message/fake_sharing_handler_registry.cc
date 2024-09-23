// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/fake_sharing_handler_registry.h"

#include "base/notreached.h"
#include "components/sharing_message/sharing_message_handler.h"

FakeSharingHandlerRegistry::FakeSharingHandlerRegistry() = default;
FakeSharingHandlerRegistry::~FakeSharingHandlerRegistry() = default;

SharingMessageHandler* FakeSharingHandlerRegistry::GetSharingHandler(
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  auto it = handler_map_.find(payload_case);
  return it != handler_map_.end() ? it->second : nullptr;
}

void FakeSharingHandlerRegistry::RegisterSharingHandler(
    std::unique_ptr<SharingMessageHandler> handler,
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  NOTIMPLEMENTED();
}

void FakeSharingHandlerRegistry::UnregisterSharingHandler(
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  NOTIMPLEMENTED();
}

void FakeSharingHandlerRegistry::SetSharingHandler(
    components_sharing_message::SharingMessage::PayloadCase payload_case,
    SharingMessageHandler* handler) {
  handler_map_[payload_case] = handler;
}

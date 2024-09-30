// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_FAKE_SHARING_HANDLER_REGISTRY_H_
#define COMPONENTS_SHARING_MESSAGE_FAKE_SHARING_HANDLER_REGISTRY_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "components/sharing_message/sharing_handler_registry.h"

class FakeSharingHandlerRegistry : public SharingHandlerRegistry {
 public:
  FakeSharingHandlerRegistry();
  FakeSharingHandlerRegistry(const FakeSharingHandlerRegistry&) = delete;
  FakeSharingHandlerRegistry& operator=(const FakeSharingHandlerRegistry&) =
      delete;
  ~FakeSharingHandlerRegistry() override;

  // SharingHandlerRegistry:
  SharingMessageHandler* GetSharingHandler(
      components_sharing_message::SharingMessage::PayloadCase payload_case)
      override;
  void RegisterSharingHandler(
      std::unique_ptr<SharingMessageHandler> handler,
      components_sharing_message::SharingMessage::PayloadCase payload_case)
      override;
  void UnregisterSharingHandler(
      components_sharing_message::SharingMessage::PayloadCase payload_case)
      override;

  void SetSharingHandler(
      components_sharing_message::SharingMessage::PayloadCase payload_case,
      SharingMessageHandler* handler);

 private:
  std::map<components_sharing_message::SharingMessage::PayloadCase,
           raw_ptr<SharingMessageHandler, CtnExperimental>>
      handler_map_;
};

#endif  // COMPONENTS_SHARING_MESSAGE_FAKE_SHARING_HANDLER_REGISTRY_H_

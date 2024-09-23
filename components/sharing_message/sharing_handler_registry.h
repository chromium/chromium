// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_HANDLER_REGISTRY_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_HANDLER_REGISTRY_H_

#include "components/sharing_message/proto/sharing_message.pb.h"

class SharingMessageHandler;

class SharingHandlerRegistry {
 public:
  SharingHandlerRegistry() = default;
  virtual ~SharingHandlerRegistry() = default;

  // Gets SharingMessageHandler registered for |payload_case|.
  virtual SharingMessageHandler* GetSharingHandler(
      components_sharing_message::SharingMessage::PayloadCase payload_case) = 0;

  // Register SharingMessageHandler for |payload_case|.
  virtual void RegisterSharingHandler(
      std::unique_ptr<SharingMessageHandler> handler,
      components_sharing_message::SharingMessage::PayloadCase payload_case) = 0;

  // Unregister SharingMessageHandler for |payload_case|.
  virtual void UnregisterSharingHandler(
      components_sharing_message::SharingMessage::PayloadCase payload_case) = 0;
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_HANDLER_REGISTRY_H_

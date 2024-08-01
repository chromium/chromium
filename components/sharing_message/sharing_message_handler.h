// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_HANDLER_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_HANDLER_H_

#include <memory>

#include "base/functional/callback.h"

namespace components_sharing_message {
class SharingMessage;
class ResponseMessage;
}  // namespace components_sharing_message

// Interface for handling incoming SharingMessage.
class SharingMessageHandler {
 public:
  using DoneCallback = base::OnceCallback<void(
      std::unique_ptr<components_sharing_message::ResponseMessage>)>;

  virtual ~SharingMessageHandler() = default;

  // Called when a SharingMessage has been received. |done_callback| must be
  // invoked after work to determine response is done.
  virtual void OnMessage(components_sharing_message::SharingMessage message,
                         DoneCallback done_callback) = 0;
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_HANDLER_H_

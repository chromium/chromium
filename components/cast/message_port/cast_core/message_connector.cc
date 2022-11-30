// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/cast_core/message_connector.h"

#include "base/check_op.h"
#include "components/cast/message_port/cast_core/message_port_core.h"

namespace cast_api_bindings {

MessageConnector::MessageConnector() : MessageConnector(0) {}
MessageConnector::MessageConnector(uint32_t channel_id)
    : channel_id_(channel_id) {}

MessageConnector::~MessageConnector() {
  if (peer_) {
    peer_->DetachPeer();
  }

  peer_ = nullptr;
}

void MessageConnector::SetPeer(MessageConnector* other) {
  DCHECK(other);
  DCHECK_EQ(channel_id_, other->channel_id());
  peer_ = other;
}

void MessageConnector::DetachPeer() {
  peer_ = nullptr;
  OnPeerError();
}

void MessageConnector::Start() {
  if (started_) {
    return;
  }

  started_ = true;
  if (peer_) {
    peer_->OnPeerStarted();
  }
}

}  // namespace cast_api_bindings

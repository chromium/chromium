// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/ping_message_handler.h"

#include "components/sharing_message/proto/sharing_message.pb.h"

PingMessageHandler::PingMessageHandler() = default;

PingMessageHandler::~PingMessageHandler() = default;

void PingMessageHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  // Delibrately empty.
  std::move(done_callback).Run(/*response=*/nullptr);
}

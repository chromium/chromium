// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/openscreen_message_port.h"

#include <string_view>
#include <utility>

#include "base/check.h"
#include "third_party/openscreen/src/cast/common/public/message_port.h"

namespace mirroring {

OpenscreenMessagePort::OpenscreenMessagePort(
    std::string_view source_id,
    std::string_view destination_id,
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
    mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel)
    : source_id_(source_id),
      destination_id_(destination_id),
      outbound_channel_(std::move(outbound_channel)),
      inbound_channel_(this, std::move(inbound_channel)) {}

OpenscreenMessagePort::~OpenscreenMessagePort() = default;

void OpenscreenMessagePort::SetClient(Client& client) {
  DCHECK_EQ(source_id_, client.source_id());
  client_ = &client;
}

void OpenscreenMessagePort::ResetClient() {
  client_ = nullptr;
}

void OpenscreenMessagePort::PostMessage(
    const std::string& destination_sender_id,
    const std::string& message_namespace,
    const std::string& message) {
  CHECK_EQ(destination_id_, destination_sender_id);
  auto message_mojom = mojom::CastMessage::New();
  message_mojom->message_namespace = message_namespace;
  message_mojom->json_format_data = message;
  outbound_channel_->OnMessage(std::move(message_mojom));
}

void OpenscreenMessagePort::OnMessage(mojom::CastMessagePtr message) {
  if (client_) {
    client_->OnMessage(destination_id_, message->message_namespace,
                       message->json_format_data);
  }
}

}  // namespace mirroring

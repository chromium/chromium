// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_MESSAGE_PORT_H_
#define COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_MESSAGE_PORT_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/openscreen/src/cast/common/public/message_port.h"

namespace mirroring {

class COMPONENT_EXPORT(MIRRORING_SERVICE) OpenscreenMessagePort final
    : public openscreen::cast::MessagePort,
      public mojom::CastMessageChannel {
 public:
  // In Chrome the source and destination are fixed for a given message port.
  OpenscreenMessagePort(
      std::string_view source_id,
      std::string_view destination_id,
      mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
      mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel);

  ~OpenscreenMessagePort() override;

  // openscreen::cast::MessagePort overrides.
  void SetClient(Client& client) override;
  void ResetClient() override;
  void PostMessage(const std::string& destination_sender_id,
                   const std::string& message_namespace,
                   const std::string& message) override;

 private:
  // mojom::CastMessageChannel implementation (inbound messages).
  void OnMessage(mojom::CastMessagePtr message) override;

  const std::string_view source_id_;
  const std::string destination_id_;
  const mojo::Remote<mojom::CastMessageChannel> outbound_channel_;
  const mojo::Receiver<mojom::CastMessageChannel> inbound_channel_;

  raw_ptr<Client> client_ = nullptr;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_MESSAGE_PORT_H_

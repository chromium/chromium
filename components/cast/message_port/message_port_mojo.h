// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_MESSAGE_PORT_MESSAGE_PORT_MOJO_H_
#define COMPONENTS_CAST_MESSAGE_PORT_MESSAGE_PORT_MOJO_H_

#include <string>

#include "components/cast/message_port/message_port.h"
#include "components/cast/message_port/mojom/unidirectional_message_port.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace cast_api_bindings {

// A MessagePort implementation built on top of a Mojo stream.
class MessagePortMojo : public mojom::UnidirectionalMessagePort,
                        public MessagePort {
 public:
  MessagePortMojo(
      mojo::PendingReceiver<mojom::UnidirectionalMessagePort> receiver,
      mojo::PendingRemote<mojom::UnidirectionalMessagePort> remote);
  ~MessagePortMojo() override;

  // MessagePort overrides.
  bool PostMessage(base::StringPiece message) override;
  bool PostMessageWithTransferables(
      base::StringPiece message,
      std::vector<std::unique_ptr<MessagePort>> ports) override;
  void SetReceiver(MessagePort::Receiver* receiver) override;
  void Close() override;
  bool CanPostMessage() const override;

 private:
  // mojom::UnidirectionalMessagePort overrides.
  void PostMessageWithTransferables(
      const std::string& message,
      std::vector<mojom::MessagePortPtr> ports) override;

  // Returns the mojo::MessagePort containing both the |sender_| and |receiver_|
  // associated with this instance, which is invalidated by this method.
  mojom::MessagePortPtr Unbind();

  // Called as a disconnect handler for |sender_| and |receiver_|.
  void OnMojoDisconnected();

  MessagePort::Receiver* receiver_ = nullptr;

  mojo::Remote<mojom::UnidirectionalMessagePort> mojo_remote_;
  mojo::Receiver<mojom::UnidirectionalMessagePort> mojo_receiver_;

  base::WeakPtrFactory<MessagePortMojo> weak_factory_;
};

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_MESSAGE_PORT_MESSAGE_PORT_MOJO_H_

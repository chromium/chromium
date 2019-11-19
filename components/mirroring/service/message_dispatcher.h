// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MESSAGE_DISPATCHER_H_
#define COMPONENTS_MIRRORING_SERVICE_MESSAGE_DISPATCHER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/service/receiver_response.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace mirroring {

// Dispatches inbound/outbound messages. The outbound messages are sent out
// through |outbound_channel|, and the inbound messages are handled by this
// class.
class COMPONENT_EXPORT(MIRRORING_SERVICE) MessageDispatcher final
    : public mojom::CastMessageChannel {
 public:
  using ErrorCallback = base::RepeatingCallback<void(const std::string&)>;
  MessageDispatcher(
      mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
      mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel,
      ErrorCallback error_callback);
  ~MessageDispatcher() override;

  using ResponseCallback =
      base::RepeatingCallback<void(const ReceiverResponse& response)>;
  // Registers/Unregisters callback for a certain type of responses.
  void Subscribe(ResponseType type, ResponseCallback callback);
  void Unsubscribe(ResponseType type);

  using OnceResponseCallback =
      base::OnceCallback<void(const ReceiverResponse& response)>;
  // Sends the given message and subscribes to replies until an acceptable one
  // is received or a timeout elapses. Message of the given response type is
  // delivered to the supplied callback if the sequence number of the response
  // matches |sequence_number|. If the timeout period elapses, the callback will
  // be run once with an unknown type of |response|.
  // Note: Calling RequestReply() before a previous reply was made will cancel
  // the previous request and not run its response callback.
  void RequestReply(mojom::CastMessagePtr message,
                    ResponseType response_type,
                    int32_t sequence_number,
                    const base::TimeDelta& timeout,
                    OnceResponseCallback callback);

  // Get the sequence number for the next outbound message. Never returns 0.
  int32_t GetNextSeqNumber();

  // Requests to send outbound |message|.
  void SendOutboundMessage(mojom::CastMessagePtr message);

 private:
  class RequestHolder;

  // mojom::CastMessageChannel implementation. Handles inbound messages.
  void Send(mojom::CastMessagePtr message) override;

  // Takes care of sending outbound messages.
  const mojo::Remote<mojom::CastMessageChannel> outbound_channel_;

  const mojo::Receiver<mojom::CastMessageChannel> receiver_;

  const ErrorCallback error_callback_;

  int32_t last_sequence_number_;

  // Holds callbacks for different types of responses.
  base::flat_map<ResponseType, ResponseCallback> callback_map_;

  DISALLOW_COPY_AND_ASSIGN(MessageDispatcher);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MESSAGE_DISPATCHER_H_

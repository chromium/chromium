// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/message_dispatcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/timer/timer.h"

namespace mirroring {

// Holds a request until |timeout| elapses or an acceptable response is
// received. When timeout, |response_callback| runs with an UNKNOWN type
// response.
class MessageDispatcher::RequestHolder {
 public:
  RequestHolder() {}

  ~RequestHolder() {}

  void Start(const base::TimeDelta& timeout,
             int32_t sequence_number,
             OnceResponseCallback response_callback) {
    response_callback_ = std::move(response_callback);
    sequence_number_ = sequence_number;
    DCHECK(!response_callback_.is_null());
    timer_.Start(
        FROM_HERE, timeout,
        base::BindRepeating(&RequestHolder::SendResponse,
                            base::Unretained(this), ReceiverResponse()));
  }

  // Send |response| if the sequence number matches, or if the request times
  // out, in which case the |response| is UNKNOWN type.
  void SendResponse(const ReceiverResponse& response) {
    if (!timer_.IsRunning() || response.sequence_number == sequence_number_)
      std::move(response_callback_).Run(response);
    // Ignore the response with mismatched sequence number.
  }

 private:
  OnceResponseCallback response_callback_;
  base::OneShotTimer timer_;
  int32_t sequence_number_ = -1;

  DISALLOW_COPY_AND_ASSIGN(RequestHolder);
};

MessageDispatcher::MessageDispatcher(
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
    mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel,
    ErrorCallback error_callback)
    : outbound_channel_(std::move(outbound_channel)),
      receiver_(this, std::move(inbound_channel)),
      error_callback_(std::move(error_callback)),
      last_sequence_number_(base::RandInt(0, 1e9)) {
  DCHECK(outbound_channel_);
  DCHECK(!error_callback_.is_null());
}

MessageDispatcher::~MessageDispatcher() {
  // Prevent the re-entrant operations on |callback_map_|.
  decltype(callback_map_) subscriptions;
  subscriptions.swap(callback_map_);
  subscriptions.clear();
}

void MessageDispatcher::Send(mojom::CastMessagePtr message) {
  if (message->message_namespace != mojom::kWebRtcNamespace &&
      message->message_namespace != mojom::kRemotingNamespace) {
    DVLOG(2) << "Ignore message with unknown namespace = "
             << message->message_namespace;
    return;  // Ignore message with wrong namespace.
  }
  if (message->json_format_data.empty())
    return;  // Ignore null message.

  ReceiverResponse response;
  if (!response.Parse(message->json_format_data)) {
    error_callback_.Run("Response parsing error. message=" +
                        message->json_format_data);
    return;
  }

#if DCHECK_IS_ON()
  if (response.type == ResponseType::RPC)
    DCHECK_EQ(mojom::kRemotingNamespace, message->message_namespace);
  else
    DCHECK_EQ(mojom::kWebRtcNamespace, message->message_namespace);
#endif  // DCHECK_IS_ON()

  const auto callback_iter = callback_map_.find(response.type);
  if (callback_iter == callback_map_.end()) {
    error_callback_.Run("No callback subscribed. message=" +
                        message->json_format_data);
    return;
  }
  callback_iter->second.Run(response);
}

void MessageDispatcher::Subscribe(ResponseType type,
                                  ResponseCallback callback) {
  DCHECK(type != ResponseType::UNKNOWN);
  DCHECK(!callback.is_null());

  const auto insert_result =
      callback_map_.emplace(std::make_pair(type, std::move(callback)));
  DCHECK(insert_result.second);
}

void MessageDispatcher::Unsubscribe(ResponseType type) {
  auto iter = callback_map_.find(type);
  if (iter != callback_map_.end())
    callback_map_.erase(iter);
}

int32_t MessageDispatcher::GetNextSeqNumber() {
  // Skip 0, which is used by Cast receiver to indicate that the broadcast
  // status message is not coming from a specific sender (it is an autonomous
  // status change, not triggered by a command from any sender). Strange usage
  // of 0 though; could be a null / optional field.
  return ++last_sequence_number_;
}

void MessageDispatcher::SendOutboundMessage(mojom::CastMessagePtr message) {
  outbound_channel_->Send(std::move(message));
}

void MessageDispatcher::RequestReply(mojom::CastMessagePtr message,
                                     ResponseType response_type,
                                     int32_t sequence_number,
                                     const base::TimeDelta& timeout,
                                     OnceResponseCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(timeout > base::TimeDelta());

  Unsubscribe(response_type);  // Cancel the old request if there is any.
  RequestHolder* const request_holder = new RequestHolder();
  request_holder->Start(
      timeout, sequence_number,
      base::BindOnce(
          [](MessageDispatcher* dispatcher, ResponseType response_type,
             OnceResponseCallback callback, const ReceiverResponse& response) {
            dispatcher->Unsubscribe(response_type);
            std::move(callback).Run(response);
          },
          this, response_type, std::move(callback)));
  // |request_holder| keeps alive until the callback is unsubscribed.
  Subscribe(response_type, base::BindRepeating(
                               [](RequestHolder* request_holder,
                                  const ReceiverResponse& response) {
                                 request_holder->SendResponse(response);
                               },
                               base::Owned(request_holder)));
  SendOutboundMessage(std::move(message));
}

}  // namespace mirroring

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_RECEIVER_SESSION_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_RECEIVER_SESSION_H_

#include <memory>

#include "base/callback.h"
#include "components/cast_streaming/public/mojom/cast_streaming_session.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace cast_api_bindings {
class MessagePort;
}

namespace cast_streaming {

// This interface handles a single Cast Streaming Receiver Session over a given
// |message_port| and with a given |cast_streaming_receiver|. On destruction,
// the Cast Streaming Receiver Session will be terminated if it was ever
// started.
class ReceiverSession {
 public:
  using MessagePortProvider =
      base::OnceCallback<std::unique_ptr<cast_api_bindings::MessagePort>()>;

  virtual ~ReceiverSession() = default;

  static std::unique_ptr<ReceiverSession> Create(
      MessagePortProvider message_port_provider);

  // Sets up the CastStreamingReceiver mojo remote. This will immediately call
  // CastStreamingReceiver::EnableReceiver(). Upon receiving the callback for
  // this method, the Cast Streaming Receiver Session will be started and audio
  // and/or video frames will be sent over a Mojo channel.
  virtual void SetCastStreamingReceiver(
      mojo::AssociatedRemote<mojom::CastStreamingReceiver>
          cast_streaming_receiver) = 0;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_RECEIVER_SESSION_H_

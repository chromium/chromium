// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(crbug.com/40181416): Delete this file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_CAST_MESSAGE_PORT_IMPL_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_CAST_MESSAGE_PORT_IMPL_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/cast/message_port/message_port.h"
#include "third_party/openscreen/src/cast/common/public/message_port.h"

namespace cast_streaming {

// Wrapper for a cast MessagePort that provides an Open Screen MessagePort
// implementation.
class CastMessagePortImpl final
    : public openscreen::cast::MessagePort,
      public cast_api_bindings::MessagePort::Receiver {
 public:
  CastMessagePortImpl(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      base::OnceClosure on_close);
  ~CastMessagePortImpl() override;

  CastMessagePortImpl(const CastMessagePortImpl&) = delete;
  CastMessagePortImpl& operator=(const CastMessagePortImpl&) = delete;

  // openscreen::cast::MessagePort implementation.
  void SetClient(Client& client) override;
  void ResetClient() override;
  void PostMessage(const std::string& sender_id,
                   const std::string& message_namespace,
                   const std::string& message) override;

 private:
  // Resets |message_port_| if it is open and signals an error to |client_| if
  // |client_| is set.
  void MaybeClose();

  // Returns a "not supported" error message to the sender for messages from
  // the inject namespace.
  void SendInjectResponse(const std::string& sender_id,
                          const std::string& message);

  // Handles messages from the media namespace. Ignores play/pause requests and
  // sends the media status as continuously playing.
  void HandleMediaMessage(const std::string& sender_id,
                          const std::string& message);

  // cast_api_bindings::MessagePort::Receiver implementation.
  bool OnMessage(std::string_view message,
                 std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                     ports) override;
  void OnPipeError() override;

  raw_ptr<Client> client_ = nullptr;
  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;
  base::OnceClosure on_close_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_CAST_MESSAGE_PORT_IMPL_H_

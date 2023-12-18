// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_TEST_CAST_MESSAGE_PORT_SENDER_IMPL_H_
#define COMPONENTS_CAST_STREAMING_TEST_CAST_MESSAGE_PORT_SENDER_IMPL_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/cast/message_port/message_port.h"
#include "third_party/openscreen/src/cast/common/public/message_port.h"

namespace cast_streaming {

// Adapter for a cast MessagePort that provides an Open Screen MessagePort
// implementation for a Cast Streaming Sender.
class CastMessagePortSenderImpl final
    : public openscreen::cast::MessagePort,
      public cast_api_bindings::MessagePort::Receiver {
 public:
  explicit CastMessagePortSenderImpl(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      base::OnceClosure on_close,
      base::OnceClosure on_system_sender_message_received);
  ~CastMessagePortSenderImpl() override;

  CastMessagePortSenderImpl(const CastMessagePortSenderImpl&) = delete;
  CastMessagePortSenderImpl& operator=(const CastMessagePortSenderImpl&) =
      delete;

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

  // cast_api_bindings::MessagePort::Receiver implementation.
  bool OnMessage(std::string_view message,
                 std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                     ports) override;
  void OnPipeError() override;

  raw_ptr<Client> client_ = nullptr;
  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;
  base::OnceClosure on_close_;
  base::OnceClosure on_system_sender_message_received_;
  bool is_closed_ = false;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_TEST_CAST_MESSAGE_PORT_SENDER_IMPL_H_

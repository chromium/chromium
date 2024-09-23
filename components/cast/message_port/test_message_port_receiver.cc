// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/test_message_port_receiver.h"

#include <string_view>

#include "base/run_loop.h"

namespace cast_api_bindings {

TestMessagePortReceiver::TestMessagePortReceiver() = default;

TestMessagePortReceiver::~TestMessagePortReceiver() = default;

void TestMessagePortReceiver::SetOnMessageResult(bool result) {
  on_message_result_ = result;
}

bool TestMessagePortReceiver::RunUntilMessageCountEqual(size_t message_count) {
  base::RunLoop run_loop;
  on_receive_satisfied_ = run_loop.QuitClosure();
  message_count_target_ = message_count;
  run_loop.Run();
  return message_count_target_ == message_count;
}

void TestMessagePortReceiver::RunUntilDisconnected() {
  base::RunLoop run_loop;
  on_disconnect_ = run_loop.QuitClosure();
  run_loop.Run();
}

bool TestMessagePortReceiver::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<MessagePort>> ports) {
  buffer_.push_back(std::make_pair(std::string(message), std::move(ports)));
  if (message_count_target_ == buffer_.size()) {
    DCHECK(on_receive_satisfied_);
    std::move(on_receive_satisfied_).Run();
  }
  return on_message_result_;
}

void TestMessagePortReceiver::OnPipeError() {
  if (on_disconnect_)
    std::move(on_disconnect_).Run();
}

}  // namespace cast_api_bindings

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/fake_ipc_server.h"

namespace named_mojo_ipc_server {

FakeIpcServer::TestState::TestState() = default;

FakeIpcServer::TestState::~TestState() = default;

FakeIpcServer::FakeIpcServer(TestState* test_state) : test_state_(test_state) {}

FakeIpcServer::~FakeIpcServer() = default;

void FakeIpcServer::StartServer() {
  test_state_->is_server_started = true;
}

void FakeIpcServer::StopServer() {
  test_state_->is_server_started = false;
}

void FakeIpcServer::Close(mojo::ReceiverId id) {
  test_state_->last_closed_receiver = id;
}

void FakeIpcServer::set_disconnect_handler(base::RepeatingClosure handler) {
  test_state_->disconnect_handler = handler;
}

mojo::ReceiverId FakeIpcServer::current_receiver() const {
  return test_state_->current_receiver;
}

const ConnectionInfo& FakeIpcServer::current_connection_info() const {
  return *test_state_->current_connection_info;
}

}  // namespace named_mojo_ipc_server

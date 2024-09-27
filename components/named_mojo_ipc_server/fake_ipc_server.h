// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_FAKE_IPC_SERVER_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_FAKE_IPC_SERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/named_mojo_ipc_server/ipc_server.h"

namespace named_mojo_ipc_server {

class FakeIpcServer final : public IpcServer {
 public:
  // Used to interact with FakeIpcServer after ownership is passed elsewhere.
  struct TestState {
    TestState();
    ~TestState();

    bool is_server_started = false;
    base::RepeatingClosure disconnect_handler;
    mojo::ReceiverId current_receiver = 0u;
    mojo::ReceiverId last_closed_receiver = 0u;
    std::unique_ptr<ConnectionInfo> current_connection_info;
  };

  explicit FakeIpcServer(TestState* test_state);
  FakeIpcServer(const FakeIpcServer&) = delete;
  FakeIpcServer& operator=(const FakeIpcServer&) = delete;
  ~FakeIpcServer() override;

  void StartServer() override;
  void StopServer() override;
  void Close(mojo::ReceiverId id) override;
  void set_disconnect_handler(base::RepeatingClosure handler) override;
  mojo::ReceiverId current_receiver() const override;
  const ConnectionInfo& current_connection_info() const override;

 private:
  raw_ptr<TestState> test_state_;
};

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_FAKE_IPC_SERVER_H_

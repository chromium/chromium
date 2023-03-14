// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/wayland_server_test.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/exo/security_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland::test {

WaylandServerTest::WaylandServerTest() = default;

WaylandServerTest::~WaylandServerTest() = default;

void WaylandServerTest::SetUp() {
  WaylandServerTestBase::SetUp();

  server_ = CreateServer();

  std::string socket_name;
  base::RunLoop loop;
  server_->StartAsync(
      base::BindLambdaForTesting([&](bool success, const base::FilePath& path) {
        DCHECK(success);
        socket_name = path.AsUTF8Unsafe();
        loop.Quit();
      }));
  loop.Run();

  client_thread_ = std::make_unique<TestWaylandClientThread>("client");
  ASSERT_TRUE(client_thread_->Start(
      base::BindOnce(&WaylandServerTest::InitOnClientThread,
                     base::Unretained(this), socket_name)));
}

void WaylandServerTest::TearDown() {
  client_thread_.reset();
  server_.reset();

  WaylandServerTestBase::TearDown();
}

void WaylandServerTest::PostToClientAndWait(
    base::OnceCallback<void(TestClient* client)> callback) {
  client_thread_->RunAndWait(std::move(callback));
}

void WaylandServerTest::PostToClientAndWait(base::OnceClosure closure) {
  client_thread_->RunAndWait(std::move(closure));
}

std::unique_ptr<TestClient> WaylandServerTest::InitOnClientThread(
    const std::string& wayland_socket) {
  auto client = std::make_unique<TestClient>();
  if (!client->Init(wayland_socket)) {
    return nullptr;
  }

  return client;
}

}  // namespace exo::wayland::test

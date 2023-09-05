// Copyright 2022 The Chromium Authors
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
#include "components/exo/wayland/server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland::test {

WaylandServerTest::WaylandServerTest() {
  Server::SetServerGetter(base::BindLambdaForTesting([&](wl_display* display) {
    // Currently tests run with a single Server instance.
    EXPECT_EQ(display, server_->GetWaylandDisplay());
    return server_.get();
  }));
}

WaylandServerTest::~WaylandServerTest() {
  Server::SetServerGetter(base::NullCallback());
}

void WaylandServerTest::SetUp() {
  WaylandServerTestBase::SetUp();

  socket_ = std::make_unique<ScopedTempSocket>();
  server_ = CreateServer();

  base::RunLoop loop;
  server_->StartWithFdAsync(socket_->TakeFd(),
                            base::BindLambdaForTesting([&](bool success) {
                              ASSERT_TRUE(success);
                              loop.Quit();
                            }));
  loop.Run();

  client_thread_ = std::make_unique<TestWaylandClientThread>("client");
  ASSERT_TRUE(client_thread_->Start(base::BindOnce(
      &WaylandServerTest::InitOnClientThread, base::Unretained(this))));
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

std::unique_ptr<TestClient> WaylandServerTest::InitOnClientThread() {
  auto client = std::make_unique<TestClient>();
  if (!client->Init(socket_->server_path().value())) {
    return nullptr;
  }

  return client;
}

}  // namespace exo::wayland::test

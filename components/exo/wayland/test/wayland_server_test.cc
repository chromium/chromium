// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/test/wayland_server_test.h"

#include <wayland-server-protocol-core.h>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "components/exo/security_delegate.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland::test {

// Waits for the server to construct client objects on the server thread
// following a connection initiated on the client thread.
class ClientCreatedWaiter {
 public:
  explicit ClientCreatedWaiter(wl_display* display) {
    listener_.notify = [](wl_listener* listener_ptr, void* data) {
      ClientCreatedWaiter* created_waiter = wl_container_of(
          listener_ptr, /*sample=*/created_waiter, /*member=*/listener_);
      created_waiter->OnClientCreated(static_cast<wl_client*>(data));
    };
    wl_display_add_client_created_listener(display, &listener_);
  }
  ClientCreatedWaiter(const ClientCreatedWaiter&) = delete;
  ClientCreatedWaiter& operator=(const ClientCreatedWaiter&) = delete;
  ~ClientCreatedWaiter() = default;

  wl_client* Wait() { return future_.Get(); }

  void OnClientCreated(wl_client* client) {
    future_.SetValue(client);
    wl_list_remove(&listener_.link);
  }

 private:
  wl_listener listener_;
  base::test::TestFuture<wl_client*> future_;
};

// Waits for the destruction of a connected client to be processed on the server
// thread.
class ClientDestroyedWaiter {
 public:
  explicit ClientDestroyedWaiter(wl_client* client) {
    future_cb_ = future_.GetCallback();

    listener_.notify = [](wl_listener* listener_ptr, void* data) {
      ClientDestroyedWaiter* destroy_waiter = wl_container_of(
          listener_ptr, /*sample=*/destroy_waiter, /*member=*/listener_);
      destroy_waiter->OnClientDestroyed();
    };
    wl_client_add_destroy_listener(client, &listener_);
  }
  ClientDestroyedWaiter(const ClientDestroyedWaiter&) = delete;
  ClientDestroyedWaiter& operator=(const ClientDestroyedWaiter&) = delete;
  ~ClientDestroyedWaiter() = default;

  void Wait() {
    ASSERT_TRUE(future_.Wait()) << "Client was not destroyed by server.";
  }

  void OnClientDestroyed() { std::move(future_cb_).Run(); }

 private:
  wl_listener listener_;
  base::OnceClosure future_cb_;
  base::test::TestFuture<void> future_;
};

WaylandServerTest::WaylandServerTest()
    : WaylandServerTestBase(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
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

  // Block on the successful construction of the client and server side objects.
  // TestWaylandClientThread::Start() will block until the initialization on the
  // client thread is done.
  ClientCreatedWaiter created_waiter(server_->GetWaylandDisplay());
  client_thread_ = std::make_unique<TestWaylandClientThread>("client");
  ASSERT_TRUE(client_thread_->Start(base::BindOnce(
      &WaylandServerTest::InitOnClientThread, base::Unretained(this))));
  client_resource_ = created_waiter.Wait();
}

void WaylandServerTest::TearDown() {
  // Force a cleanup of any remaining outputs, which are deleted on a delay to
  // give clients the opportunity to release the global.
  task_environment()->FastForwardBy(WaylandDisplayOutput::kDeleteTaskDelay *
                                    (WaylandDisplayOutput::kDeleteRetries + 1));

  client_resource_ = nullptr;
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

void WaylandServerTest::DisconnectClientAndWait() {
  ClientDestroyedWaiter waiter(client_resource_);
  client_resource_ = nullptr;
  client_thread_.reset();
  waiter.Wait();
}

std::unique_ptr<TestClient> WaylandServerTest::InitOnClientThread() {
  auto client = std::make_unique<TestClient>();
  if (!client->Init(socket_->server_path().value())) {
    return nullptr;
  }

  return client;
}

}  // namespace exo::wayland::test

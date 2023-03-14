// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_WAYLAND_SERVER_TEST_H_
#define COMPONENTS_EXO_WAYLAND_TEST_WAYLAND_SERVER_TEST_H_

#include <memory>
#include <type_traits>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/test/test_client.h"
#include "components/exo/wayland/test/test_wayland_client_thread.h"
#include "components/exo/wayland/test/wayland_server_test_base.h"

namespace exo::wayland::test {

// WaylandServerTest is a gtest fixture base class used for testing Wayland
// server-side logic and exercise the Wayland protocol, running a test client
// on a dedicated thread.
class WaylandServerTest : public WaylandServerTestBase {
 public:
  WaylandServerTest(const WaylandServerTest&) = delete;
  WaylandServerTest& operator=(const WaylandServerTest&) = delete;

 protected:
  WaylandServerTest();

  // Constructs a WaylandServerTest with |traits| being forwarded to its
  // TaskEnvironment. See the corresponding |WaylandServerTestBase| constructor.
  template <typename... TaskEnvironmentTraits>
  explicit WaylandServerTest(TaskEnvironmentTraits&&... traits)
      : WaylandServerTestBase(std::forward<TaskEnvironmentTraits>(traits)...) {}

  ~WaylandServerTest() override;

  // WaylandServerTestBase:
  void SetUp() override;
  void TearDown() override;

  // Posts `callback` or `closure` to run on the client thread; blocks till the
  // callable is run and all pending Wayland requests and events are delivered.
  void PostToClientAndWait(
      base::OnceCallback<void(TestClient* client)> callback);
  void PostToClientAndWait(base::OnceClosure closure);

  // Similar to the two methods above, but provides the convenience of using a
  // capturing lambda directly.
  template <typename Lambda,
            typename = std::enable_if_t<
                std::is_invocable_r_v<void, Lambda, TestClient*> ||
                std::is_invocable_r_v<void, Lambda>>>
  void PostToClientAndWait(Lambda&& lambda) {
    PostToClientAndWait(base::BindLambdaForTesting(std::move(lambda)));
  }

  // Subclasses can override this method to create a TestClient subclass
  // instance or customize client configuration if needed.
  // This method is run on the client thread.
  virtual std::unique_ptr<TestClient> InitOnClientThread(
      const std::string& wayland_socket);

  std::unique_ptr<Server> server_;

  std::unique_ptr<TestWaylandClientThread> client_thread_;
};

}  // namespace exo::wayland::test

#endif  // COMPONENTS_EXO_WAYLAND_TEST_WAYLAND_SERVER_TEST_H_

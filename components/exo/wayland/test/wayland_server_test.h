// Copyright 2022 The Chromium Authors
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
  WaylandServerTest();
  WaylandServerTest(const WaylandServerTest&) = delete;
  WaylandServerTest& operator=(const WaylandServerTest&) = delete;
  ~WaylandServerTest() override;

  // WaylandServerTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
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

  // Initiates a client disconnect from the client thread. Waits until the
  // disconnect has been processed on the server thread.
  void DisconnectClientAndWait();

  // Subclasses can override this method to create a TestClient subclass
  // instance or customize client configuration if needed.
  // This method is run on the client thread.
  virtual std::unique_ptr<TestClient> InitOnClientThread();

  std::unique_ptr<ScopedTempSocket> socket_;
  std::unique_ptr<Server> server_;

  std::unique_ptr<TestWaylandClientThread> client_thread_;
  raw_ptr<wl_client> client_resource_;
};

}  // namespace exo::wayland::test

#endif  // COMPONENTS_EXO_WAYLAND_TEST_WAYLAND_SERVER_TEST_H_

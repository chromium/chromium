// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_TEST_WAYLAND_CLIENT_THREAD_H_
#define COMPONENTS_EXO_WAYLAND_TEST_TEST_WAYLAND_CLIENT_THREAD_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/message_loop/message_pump_epoll.h"
#include "base/threading/thread.h"
#include "components/exo/wayland/test/test_client.h"

namespace exo::wayland::test {

// TestWaylandClientThread runs a Wayland client on a dedicated thread for
// testing with WaylandServerTest.
class TestWaylandClientThread : public base::Thread,
                                base::MessagePumpEpoll::FdWatcher {
 public:
  explicit TestWaylandClientThread(const std::string& name);

  TestWaylandClientThread(const TestWaylandClientThread&) = delete;
  TestWaylandClientThread& operator=(const TestWaylandClientThread&) = delete;

  ~TestWaylandClientThread() override;

  using InitCallback = base::OnceCallback<std::unique_ptr<TestClient>()>;
  // Starts the client thread; initializes `client` by calling `init_callback`
  // on the client thread. This method blocks until the initialization on the
  // client thread is done.
  //
  // Returns false on failure. In that case, the other public APIs of this class
  // are not supposed to be called.
  bool Start(InitCallback init_callback);

  // Runs `callback` or `closure` on the client thread; blocks until the
  // callable is run and all pending Wayland requests and events are delivered.
  void RunAndWait(base::OnceCallback<void(TestClient*)> callback);
  void RunAndWait(base::OnceClosure closure);

 private:
  // base::MessagePumpEpoll::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  void DoInit(InitCallback init_callback);
  void DoRun(base::OnceClosure closure);
  void DoCleanUp();

  base::MessagePumpEpoll::FdWatchController controller_;
  std::unique_ptr<TestClient> client_;

  bool stopped_ = false;
};

}  // namespace exo::wayland::test

#endif  // COMPONENTS_EXO_WAYLAND_TEST_TEST_WAYLAND_CLIENT_THREAD_H_

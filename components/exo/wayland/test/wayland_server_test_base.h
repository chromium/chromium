// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_WAYLAND_SERVER_TEST_BASE_H_
#define COMPONENTS_EXO_WAYLAND_TEST_WAYLAND_SERVER_TEST_BASE_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "components/exo/display.h"
#include "components/exo/test/exo_test_base.h"

namespace exo {
class SecurityDelegate;
class Display;

namespace wayland {
class Server;

namespace test {

// Use ExoTestBase because Server starts to depends on ash::Shell.
using TestBase = exo::test::ExoTestBase;

// Base class for tests that create an exo's wayland server.
class WaylandServerTestBase : public TestBase {
 public:
  // When using on-demand sockets (described on go/securer-exo-ids) the server
  // does not own the socket and will not clean it up. This class is used to
  // help manage creation/cleanup of such sockets
  class ScopedTempSocket {
   public:
    ScopedTempSocket();

    // Not copyable.
    ScopedTempSocket(const ScopedTempSocket&) = delete;
    ScopedTempSocket& operator=(const ScopedTempSocket&) = delete;

    ~ScopedTempSocket();

    // Once created, this class owns the socket FD. Use this method to move the
    // FD out of here (and into a wayland server).
    base::ScopedFD TakeFd();

    const base::FilePath& server_path() const { return server_path_; }

   private:
    base::ScopedTempDir socket_dir_;
    base::FilePath server_path_;
    base::ScopedFD fd_;
  };

  WaylandServerTestBase();

  // Constructs a WaylandServerTestBase with |traits| being forwarded to its
  // TaskEnvironment. See the corresponding |AshTestBase| constructor.
  template <typename... TaskEnvironmentTraits>
  explicit WaylandServerTestBase(TaskEnvironmentTraits&&... traits)
      : TestBase(std::forward<TaskEnvironmentTraits>(traits)...) {}

  WaylandServerTestBase(const WaylandServerTestBase&) = delete;
  WaylandServerTestBase& operator=(const WaylandServerTestBase&) = delete;
  ~WaylandServerTestBase() override;

  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<Server> CreateServer(
      std::unique_ptr<SecurityDelegate> security_delegate);
  std::unique_ptr<Server> CreateServer();

 protected:
  std::unique_ptr<Display> display_;
  base::ScopedTempDir xdg_temp_dir_;
};

}  // namespace test
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_TEST_WAYLAND_SERVER_TEST_BASE_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_WAYLAND_SERVER_TEST_BASE_H_
#define COMPONENTS_EXO_WAYLAND_TEST_WAYLAND_SERVER_TEST_BASE_H_

#include <memory>

#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/exo/test/exo_test_base.h"
#else
#include "components/exo/test/exo_test_base_views.h"
#endif

namespace exo {
class Capabilities;
class Display;

namespace wayland {
class Server;

namespace test {

// Use ExoTestBase on Chrome OS because Server starts to depends on ash::Shell,
// which is unavailable on other platforms so then ExoTestBaseViews instead.
using TestBase =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    exo::test::ExoTestBase
#else
    exo::test::ExoTestBaseViews
#endif
    ;

// Base class for tests that create an exo's wayland server.
class WaylandServerTestBase : public TestBase {
 public:
  static std::string GetUniqueSocketName();

  WaylandServerTestBase();
  WaylandServerTestBase(const WaylandServerTestBase&) = delete;
  WaylandServerTestBase& operator=(const WaylandServerTestBase&) = delete;
  ~WaylandServerTestBase() override;

  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<Server> CreateServer(
      std::unique_ptr<Capabilities> capabilities);
  std::unique_ptr<Server> CreateServer();

 protected:
  std::unique_ptr<Display> display_;
  base::ScopedTempDir xdg_temp_dir_;
};

// A class to support a client side code on a separate thread.
class WaylandClientRunner : base::Thread {
 public:
  WaylandClientRunner(Server* server, const std::string& name);
  WaylandClientRunner(const WaylandClientRunner&) = delete;
  WaylandClientRunner& operator=(const WaylandClientRunner&) = delete;
  ~WaylandClientRunner() override = default;

  void RunAndWait(base::OnceClosure callback);

 private:
  Server* server_;  // not owned. server must outlive WaylandClientRunner.
  base::WaitableEvent event_;
};

}  // namespace test
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_TEST_WAYLAND_SERVER_TEST_BASE_H_

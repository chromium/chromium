// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/exo/wayland/clients/test/wayland_client_test_server.h"
#include "components/exo/wayland/compatibility_test/client_compatibility_test.h"

namespace exo {
namespace wayland {
namespace compatibility {
namespace test {
namespace {

class CompatibilityTestSuiteServer : public WaylandClientTestSuiteServer {
 public:
  using WaylandClientTestSuiteServer::WaylandClientTestSuiteServer;

  CompatibilityTestSuiteServer(const CompatibilityTestSuiteServer&) = delete;
  CompatibilityTestSuiteServer& operator=(const CompatibilityTestSuiteServer&) =
      delete;

  void SetClientTestUIThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
      override {
    ClientCompatibilityTest::SetUIThreadTaskRunner(
        std::move(ui_thread_task_runner));
  }
};

std::unique_ptr<WaylandClientTestSuiteServer> MakeServer(int argc,
                                                         char** argv) {
  return std::make_unique<CompatibilityTestSuiteServer>(argc, argv);
}

}  // namespace
}  // namespace test
}  // namespace compatibility
}  // namespace wayland
}  // namespace exo

int main(int argc, char** argv) {
  return exo::WaylandClientTestSuiteServer::TestMain(
      argc, argv,
      base::BindOnce(&exo::wayland::compatibility::test::MakeServer));
}

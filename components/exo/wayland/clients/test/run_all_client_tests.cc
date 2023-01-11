// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/exo/wayland/clients/test/wayland_client_test.h"
#include "components/exo/wayland/clients/test/wayland_client_test_server.h"

namespace exo {
namespace {

class ExoClientPerfTestSuite : public WaylandClientTestSuiteServer {
  using WaylandClientTestSuiteServer::WaylandClientTestSuiteServer;

  ExoClientPerfTestSuite(const ExoClientPerfTestSuite&) = delete;
  ExoClientPerfTestSuite& operator=(const ExoClientPerfTestSuite&) = delete;

  void SetClientTestUIThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
      override {
    WaylandClientTest::SetUIThreadTaskRunner(std::move(ui_thread_task_runner));
  }
};

std::unique_ptr<WaylandClientTestSuiteServer> MakeServer(int argc,
                                                         char** argv) {
  return std::make_unique<exo::ExoClientPerfTestSuite>(argc, argv);
}

}  // namespace
}  // namespace exo

int main(int argc, char** argv) {
  return exo::ExoClientPerfTestSuite::TestMain(
      argc, argv, base::BindOnce(&exo::MakeServer));
}

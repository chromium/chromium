// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_suite.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/exo/wayland/clients/test/wayland_client_test.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "mojo/core/embedder/embedder.h"

namespace exo {
namespace {

const char kRunWithExternalWaylandServer[] = "run-with-external-wayland-server";

class ExoClientPerfTestSuite : public ash::AshTestSuite {
 public:
  ExoClientPerfTestSuite(int argc, char** argv)
      : ash::AshTestSuite(argc, argv),
        run_with_external_wayland_server_(
            base::CommandLine::ForCurrentProcess()->HasSwitch(
                kRunWithExternalWaylandServer)) {}

  int Run() {
    Initialize();

    base::Thread client_thread("ClientThread");
    client_thread.Start();

    base::RunLoop run_loop;
    client_thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ExoClientPerfTestSuite::RunTestsOnClientThread,
                       base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();

    Shutdown();
    return result_;
  }

 private:
  // Overriden from ash::AshTestSuite:
  void Initialize() override {
    if (!base::debug::BeingDebugged())
      base::RaiseProcessToHighPriority();

    if (run_with_external_wayland_server_) {
      base::TestSuite::Initialize();

      task_environment_ = std::make_unique<base::test::TaskEnvironment>(
          base::test::TaskEnvironment::MainThreadType::UI);
    } else {
      // We only need initialized ash related stuff for running wayland server
      // within the test.
      ash::AshTestSuite::Initialize();

      // Initialize task envrionment here instead of Test::SetUp(), because all
      // tests and their SetUp() will be running in client thread.
      task_environment_ = std::make_unique<base::test::TaskEnvironment>(
          base::test::TaskEnvironment::MainThreadType::UI);

      // Set the UI thread task runner to WaylandClientTest, so all tests can
      // post tasks to UI thread.
      WaylandClientTest::SetUIThreadTaskRunner(
          base::ThreadTaskRunnerHandle::Get());
    }
  }

  void Shutdown() override {
    if (run_with_external_wayland_server_) {
      task_environment_ = nullptr;
      base::TestSuite::Shutdown();
    } else {
      WaylandClientTest::SetUIThreadTaskRunner(nullptr);
      task_environment_ = nullptr;
      ash::AshTestSuite::Shutdown();
    }
  }

 private:
  void RunTestsOnClientThread(const base::Closure& finished_closure) {
    result_ = RUN_ALL_TESTS();
    finished_closure.Run();
  }

  // Do not run the wayland server within the test.
  const bool run_with_external_wayland_server_ = false;

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

  // Result of RUN_ALL_TESTS().
  int result_ = 1;

  DISALLOW_COPY_AND_ASSIGN(ExoClientPerfTestSuite);
};

}  // namespace
}  // namespace exo

int main(int argc, char** argv) {
  mojo::core::Init();

  // The TaskEnvironment and UI thread don't get reset between tests so don't
  // reset the GPU thread either. Destroying the GPU service is problematic
  // because tasks on the UI thread can live on past the end of the test and
  // keep references to GPU thread objects.
  viz::TestGpuServiceHolder::DoNotResetOnTestExit();

  exo::ExoClientPerfTestSuite test_suite(argc, argv);

  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&exo::ExoClientPerfTestSuite::Run,
                     base::Unretained(&test_suite)));
}

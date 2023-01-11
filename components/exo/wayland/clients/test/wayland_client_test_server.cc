// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/test/wayland_client_test_server.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/functional/bind.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/exo/wayland/clients/test/wayland_client_test.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "mojo/core/embedder/embedder.h"

namespace exo {
namespace {

const char kRunWithExternalWaylandServer[] = "run-with-external-wayland-server";

}

WaylandClientTestSuiteServer::WaylandClientTestSuiteServer(int argc,
                                                           char** argv)
    : ash::AshTestSuite(argc, argv),
      run_with_external_wayland_server_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              kRunWithExternalWaylandServer)) {}

WaylandClientTestSuiteServer::~WaylandClientTestSuiteServer() = default;

int WaylandClientTestSuiteServer::Run() {
  Initialize();

  base::Thread client_thread("ClientThread");
  client_thread.Start();

  // |task_environment_| main thread will be ClientThread while running tests.
  task_environment_->DetachFromThread();

  base::RunLoop run_loop;
  client_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandClientTestSuiteServer::RunTestsOnClientThread,
                     base::Unretained(this), run_loop.QuitWhenIdleClosure()));
  run_loop.Run();

  // |task_environment_| destruction will be performed on the test suite's main
  // thread, detach from the ClientThread.
  task_environment_->DetachFromThread();

  Shutdown();
  return result_;
}

void WaylandClientTestSuiteServer::Initialize() {
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

    // Initialize task environment here instead of Test::SetUp(), because all
    // tests and their SetUp() will be running in client thread.
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::UI);

    // Set the UI thread task runner to WaylandClientTest, so all tests can
    // post tasks to UI thread.
    WaylandClientTest::SetUIThreadTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

void WaylandClientTestSuiteServer::Shutdown() {
  if (run_with_external_wayland_server_) {
    task_environment_.reset();
    base::TestSuite::Shutdown();
  } else {
    WaylandClientTest::SetUIThreadTaskRunner(nullptr);
    task_environment_.reset();
    ash::AshTestSuite::Shutdown();
  }
}

void WaylandClientTestSuiteServer::RunTestsOnClientThread(
    base::OnceClosure finished_closure) {
  result_ = RUN_ALL_TESTS();
  std::move(finished_closure).Run();
}

int WaylandClientTestSuiteServer::TestMain(
    int argc,
    char** argv,
    WaylandClientTestSuiteServer::CreateServerCallback create_server_callback) {
  mojo::core::Init();

  // The TaskEnvironment and UI thread don't get reset between tests so don't
  // reset the GPU thread either. Destroying the GPU service is problematic
  // because tasks on the UI thread can live on past the end of the test and
  // keep references to GPU thread objects.
  viz::TestGpuServiceHolder::DoNotResetOnTestExit();

  std::unique_ptr<WaylandClientTestSuiteServer> server =
      std::move(create_server_callback).Run(argc, argv);

  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&WaylandClientTestSuiteServer::Run,
                     base::Unretained(server.get())));
}

}  // namespace exo

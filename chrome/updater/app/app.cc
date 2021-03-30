// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

constexpr base::StringPiece App::kThreadPoolName;

App::App() : updater_scope_(GetProcessScope()) {}
App::~App() = default;

void App::InitializeThreadPool() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(kThreadPoolName);
}

int App::Run() {
  InitializeThreadPool();
  Initialize();
  int exit_code = 0;
  {
    base::ScopedDisallowBlocking no_blocking_allowed_on_ui_thread;
    base::RunLoop runloop;
    quit_ = base::BindOnce(
        [](base::OnceClosure quit, int* exit_code_out, int exit_code) {
          *exit_code_out = exit_code;
          std::move(quit).Run();
        },
        runloop.QuitWhenIdleClosure(), &exit_code);
    FirstTaskRun();
    runloop.Run();
  }
  Uninitialize();

  // Shutting down the thread pool involves joining threads.
  base::ThreadPoolInstance::Get()->Shutdown();
  return exit_code;
}

void App::Shutdown(int exit_code) {
  std::move(quit_).Run(exit_code);
}

UpdaterScope App::updater_scope() const {
  return updater_scope_;
}

}  // namespace updater

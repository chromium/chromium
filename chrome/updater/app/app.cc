// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace updater {

App::App() = default;
App::~App() = default;

int App::Initialize() {
  return kErrorOk;
}

int App::Run() {
  int exit_code = Initialize();
  if (exit_code != kErrorOk) {
    return exit_code;
  }
  absl::Cleanup uninitialize = [this] { Uninitialize(); };
  return RunTasks();
}

int App::RunTasks() {
  int exit_code = kErrorOk;
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
  return exit_code;
}

void App::Shutdown(int exit_code) {
  if (quit_.is_null()) {
    // It's possible for shutdown to be called twice, since the runloop exits
    // only when idle. The exit code of the first shutdown will be used.
    return;
  }

  // TODO(crbug.com/40259598): for non-silent scenarios where UI is not
  // otherwise shown, some UI is needed here if exit_code indicates a failure.
  std::move(quit_).Run(exit_code);
}

UpdaterScope App::updater_scope() const {
  return updater_scope_;
}

}  // namespace updater

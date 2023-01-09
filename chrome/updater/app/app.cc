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
#include "chrome/updater/updater_scope.h"

namespace updater {

App::App() = default;
App::~App() = default;

int App::Run() {
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
  return exit_code;
}

void App::Shutdown(int exit_code) {
  CHECK(!quit_.is_null()) << "App was shutdown previously.";
  std::move(quit_).Run(exit_code);
}

UpdaterScope App::updater_scope() const {
  return updater_scope_;
}

}  // namespace updater

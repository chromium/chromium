// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/app/app.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"

namespace enterprise_companion {

App::App() = default;
App::~App() = default;

EnterpriseCompanionStatus App::Run() {
  EnterpriseCompanionStatus status = EnterpriseCompanionStatus::Success();
  base::RunLoop run_loop;
  quit_ =
      base::BindOnce(
          [](EnterpriseCompanionStatus* status_out,
             const EnterpriseCompanionStatus& status) { *status_out = status; },
          &status)
          .Then(run_loop.QuitClosure());
  FirstTaskRun();
  run_loop.Run();
  return status;
}

void App::Shutdown(const EnterpriseCompanionStatus& status) {
  // It's possible for shutdown to be called twice, since the runloop exits
  // only when idle. The status of the first shutdown will be used.
  if (quit_) {
    std::move(quit_).Run(status);
  }
}

}  // namespace enterprise_companion

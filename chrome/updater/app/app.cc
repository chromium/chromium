// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

constexpr base::StringPiece App::kThreadPoolName;

App::App()
    : process_scope_(GetProcessScope()),
      tag_args_([]() -> absl::optional<tagging::TagArgs> {
        base::CommandLine* command_line =
            base::CommandLine::ForCurrentProcess();
        const std::string tag = command_line->GetSwitchValueASCII(kTagSwitch);
        if (tag.empty())
          return absl::nullopt;
        tagging::TagArgs tag_args;
        const tagging::ErrorCode error =
            tagging::Parse(tag, absl::nullopt, &tag_args);
        VLOG_IF(1, error != tagging::ErrorCode::kSuccess)
            << "Tag parsing returned " << error << ".";
        return error == tagging::ErrorCode::kSuccess
                   ? absl::make_optional(tag_args)
                   : absl::nullopt;
      }()) {}

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
  // TODO(crbug.com/1208946): handle conflicts between NeedAdmin and --system.
  if (tag_args_ && !tag_args_->apps.empty() &&
      tag_args_->apps.front().needs_admin) {
    // TODO(crbug.com/1128631): support bundles. For now, assume one app.
    DCHECK_EQ(tag_args_->apps.size(), size_t{1});
    switch (*tag_args_->apps.front().needs_admin) {
      case tagging::AppArgs::NeedsAdmin::kYes:
      case tagging::AppArgs::NeedsAdmin::kPrefers:
        return UpdaterScope::kSystem;
      case tagging::AppArgs::NeedsAdmin::kNo:
        return UpdaterScope::kUser;
    }
  }

  return process_scope_;
}

absl::optional<tagging::TagArgs> App::tag_args() const {
  return tag_args_;
}

}  // namespace updater

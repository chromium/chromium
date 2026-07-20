// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/compute_app_size_command.h"

#include <memory>
#include <utility>

#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/compute_app_size_job.h"
#include "chrome/browser/web_applications/jobs/get_progressive_web_app_size_job.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_isolation_delegate.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

ComputeAppSizeCommand::ComputeAppSizeCommand(
    const webapps::AppId& app_id,
    Profile* profile,
    base::OnceCallback<void(std::optional<ComputedAppSizeWithOrigin>)> callback)
    : WebAppCommand<AppLock, std::optional<ComputedAppSizeWithOrigin>>(
          "ComputeAppSizeCommand",
          AppLockDescription(app_id),
          std::move(callback),
          /*args_for_shutdown=*/
          ComputedAppSizeWithOrigin()),
      app_id_(app_id),
      profile_(profile) {
  GetMutableDebugValue().Set("app_id", app_id);
}

ComputeAppSizeCommand::~ComputeAppSizeCommand() = default;

void ComputeAppSizeCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  const WebAppRegistrar& registrar = lock_->registrar();
  if (!registrar.AppMatches(app_id_, WebAppFilter::IsAppSurfaceableToUser())) {
    ReportResultAndDestroy(CommandResult::kFailure);
    return;
  }

  if (registrar.AppMatches(app_id_, WebAppFilter::IsIsolatedApp())) {
    job_ = lock_->isolation_delegate().CreateComputeAppSizeJob(
        app_id_, GetMutableDebugValue());
  } else {
    // If an app is not an IWA, it's considerered to be a PWA.
    job_ = std::make_unique<GetProgressiveWebAppSizeJob>(
        profile_.get(), app_id_, GetMutableDebugValue());
  }

  job_->Start(lock_.get(),
              base::BindOnce(&ComputeAppSizeCommand::OnAppSizeComputed,
                             weak_factory_.GetWeakPtr()));
}

void ComputeAppSizeCommand::OnAppSizeComputed(
    std::optional<ComputedAppSizeWithOrigin> result) {
  if (result) {
    size_ = std::move(result.value());
  }
  ReportResultAndDestroy(result ? CommandResult::kSuccess
                                : CommandResult::kFailure);
}

void ComputeAppSizeCommand::ReportResultAndDestroy(CommandResult result) {
  CompleteAndSelfDestruct(result, result == CommandResult::kSuccess
                                      ? std::move(size_)
                                      : ComputedAppSizeWithOrigin());
}

}  // namespace web_app

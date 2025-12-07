// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/compute_app_size_command.h"

#include <memory>
#include <utility>

#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/get_isolated_web_app_browsing_data_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/jobs/get_isolated_web_app_size_job.h"
#include "chrome/browser/web_applications/jobs/get_progressive_web_app_size_job.h"
#include "chrome/browser/web_applications/web_app_filter.h"
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
  if (!registrar.IsInRegistrar(app_id_)) {
    ReportResultAndDestroy(CommandResult::kFailure);
    return;
  }

  if (registrar.AppMatches(app_id_, WebAppFilter::IsIsolatedApp())) {
    get_isolated_web_app_size_job_ = std::make_unique<GetIsolatedWebAppSizeJob>(
        profile_.get(), app_id_, GetMutableDebugValue(),
        base::BindOnce(&ComputeAppSizeCommand::OnIsolatedAppSizeComputed,
                       weak_factory_.GetWeakPtr()));
    get_isolated_web_app_size_job_->Start(lock_.get());
    return;
  }

  // If an app is not an IWA, it's considerered to be a PWA.
  get_progressive_web_app_size_job_ =
      std::make_unique<GetProgressiveWebAppSizeJob>(
          profile_.get(), app_id_, GetMutableDebugValue(),
          base::BindOnce(&ComputeAppSizeCommand::OnProgressiveAppSizeComputed,
                         weak_factory_.GetWeakPtr()));

  get_progressive_web_app_size_job_->Start(lock_.get());
}

void ComputeAppSizeCommand::OnIsolatedAppSizeComputed(
    std::optional<ComputedAppSizeWithOrigin> result) {
  if (result) {
    size_ = std::move(result.value());
  }
  ReportResultAndDestroy(result ? CommandResult::kSuccess
                                : CommandResult::kFailure);
}

void ComputeAppSizeCommand::OnProgressiveAppSizeComputed(
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

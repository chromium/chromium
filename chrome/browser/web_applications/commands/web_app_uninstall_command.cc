// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace web_app {

WebAppUninstallCommand::WebAppUninstallCommand(
    std::unique_ptr<UninstallJob> job,
    UninstallJob::Callback callback)
    : WebAppCommandTemplate<AllAppsLock>("WebAppUninstallCommand"),
      lock_description_(std::make_unique<AllAppsLockDescription>()),
      job_(std::move(job)),
      callback_(std::move(callback)) {
  webapps::InstallableMetrics::TrackUninstallEvent(job_->uninstall_source());
}

WebAppUninstallCommand::~WebAppUninstallCommand() = default;

void WebAppUninstallCommand::StartWithLock(std::unique_ptr<AllAppsLock> lock) {
  lock_ = std::move(lock);
  job_->Start(*lock_,
              base::BindOnce(&WebAppUninstallCommand::CompleteAndSelfDestruct,
                             weak_factory_.GetWeakPtr()));
}

void WebAppUninstallCommand::OnShutdown() {
  CompleteAndSelfDestruct(webapps::UninstallResultCode::kShutdown);
}

const LockDescription& WebAppUninstallCommand::lock_description() const {
  return *lock_description_;
}

base::Value WebAppUninstallCommand::ToDebugValue() const {
  return job_->ToDebugValue();
}

void WebAppUninstallCommand::CompleteAndSelfDestruct(
    webapps::UninstallResultCode code) {
  CHECK(callback_);
  base::UmaHistogramBoolean("WebApp.Uninstall.Result",
                            UninstallSucceeded(code));
  SignalCompletionAndSelfDestruct(
      [code]() {
        switch (code) {
          case webapps::UninstallResultCode::kSuccess:
          case webapps::UninstallResultCode::kNoAppToUninstall:
            return CommandResult::kSuccess;
          case webapps::UninstallResultCode::kCancelled:
          case webapps::UninstallResultCode::kError:
            return CommandResult::kFailure;
          case webapps::UninstallResultCode::kShutdown:
            return CommandResult::kShutdown;
        }
      }(),
      base::BindOnce(std::move(callback_), code));
}

}  // namespace web_app

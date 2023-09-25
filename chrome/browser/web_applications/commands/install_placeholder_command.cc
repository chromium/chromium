// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_placeholder_command.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/jobs/uninstall/web_app_uninstall_and_replace_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/install_result_code.h"

namespace web_app {

InstallPlaceholderCommand::InstallPlaceholderCommand(
    Profile* profile,
    const ExternalInstallOptions& install_options,
    InstallAndReplaceCallback callback)
    : WebAppCommandTemplate<SharedWebContentsWithAppLock>(
          "InstallPlaceholderCommand"),
      profile_(profile),
      // For placeholder installs, the install_url is treated as the start_url.
      app_id_(GenerateAppIdFromManifestId(
          GenerateManifestIdFromStartUrlOnly(install_options.install_url))),
      lock_description_(
          std::make_unique<SharedWebContentsWithAppLockDescription>(
              base::flat_set<webapps::AppId>{app_id_})),
      install_options_(install_options),
      callback_(std::move(callback)) {}

InstallPlaceholderCommand::~InstallPlaceholderCommand() = default;

void InstallPlaceholderCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsWithAppLock> lock) {
  lock_ = std::move(lock);
  install_placeholder_job_.emplace(
      profile_.get(), install_options_,
      base::BindOnce(&InstallPlaceholderCommand::OnPlaceholderInstalled,
                     weak_factory_.GetWeakPtr()),
      *lock_);
  if (data_retriever_for_testing_) {
    install_placeholder_job_->SetDataRetrieverForTesting(
        std::move(data_retriever_for_testing_));
  }
  install_placeholder_job_->Start();
}

const LockDescription& InstallPlaceholderCommand::lock_description() const {
  return *lock_description_;
}

base::Value InstallPlaceholderCommand::ToDebugValue() const {
  base::Value::Dict dict;
  dict.Set("install_placeholder_job",
           install_placeholder_job_ ? install_placeholder_job_->ToDebugValue()
                                    : base::Value());
  dict.Set("uninstall_and_replace_job",
           uninstall_and_replace_job_
               ? uninstall_and_replace_job_->ToDebugValue()
               : base::Value());
  return base::Value(std::move(dict));
}

void InstallPlaceholderCommand::OnShutdown() {
  Abort(webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

void InstallPlaceholderCommand::SetDataRetrieverForTesting(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  data_retriever_for_testing_ = std::move(data_retriever);
}

void InstallPlaceholderCommand::Abort(webapps::InstallResultCode code) {
  webapps::InstallableMetrics::TrackInstallResult(false);
  if (!callback_) {
    return;
  }

  SignalCompletionAndSelfDestruct(
      (code ==
       webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown)
          ? CommandResult::kShutdown
          : CommandResult::kFailure,
      base::BindOnce(std::move(callback_),
                     ExternallyManagedAppManager::InstallResult(
                         code, app_id_,
                         /*did_uninstall_and_replace=*/false)));
}

void InstallPlaceholderCommand::OnPlaceholderInstalled(
    webapps::InstallResultCode code,
    webapps::AppId app_id) {
  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code));

  if (!callback_) {
    return;
  }

  if (!webapps::IsSuccess(code)) {
    Abort(code);
    return;
  }

  uninstall_and_replace_job_.emplace(
      profile_.get(), *lock_, install_options_.uninstall_and_replace, app_id_,
      base::BindOnce(&InstallPlaceholderCommand::OnUninstallAndReplaced,
                     weak_factory_.GetWeakPtr(), code));
  uninstall_and_replace_job_->Start();
}

void InstallPlaceholderCommand::OnUninstallAndReplaced(
    webapps::InstallResultCode code,
    bool did_uninstall_and_replace) {
  SignalCompletionAndSelfDestruct(
      CommandResult::kSuccess,
      base::BindOnce(
          std::move(callback_),
          ExternallyManagedAppManager::InstallResult(
              std::move(code), std::move(app_id_), did_uninstall_and_replace)));
}

}  // namespace web_app

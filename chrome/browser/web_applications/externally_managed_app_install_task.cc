// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_install_task.h"

#include <utility>

#include "chrome/browser/web_applications/commands/external_app_resolution_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

ExternallyManagedAppInstallTask::ExternallyManagedAppInstallTask(
    WebAppProvider& provider,
    ExternalInstallOptions install_options)
    : provider_(provider), install_options_(std::move(install_options)) {}

ExternallyManagedAppInstallTask::~ExternallyManagedAppInstallTask() = default;

void ExternallyManagedAppInstallTask::Install(
    std::optional<webapps::AppId> installed_placeholder_app_id,
    ResultCallback result_callback) {
  provider_->scheduler().InstallExternallyManagedApp(
      std::move(install_options_), std::move(installed_placeholder_app_id),
      std::move(result_callback));
}

}  // namespace web_app

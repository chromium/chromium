// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/web_applications/web_app_command_scheduler.h"

#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

WebAppCommandScheduler::WebAppCommandScheduler(WebAppProvider* provider)
    : provider_(provider) {}

WebAppCommandScheduler::~WebAppCommandScheduler() = default;

void WebAppCommandScheduler::FetchManifestAndInstall(
    webapps::WebappInstallSource install_surface,
    base::WeakPtr<content::WebContents> contents,
    bool bypass_service_worker_check,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback,
    bool use_fallback) {
  if (!provider_->is_registry_ready()) {
    provider_->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&WebAppCommandScheduler::FetchManifestAndInstall,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(install_surface), std::move(contents),
                       bypass_service_worker_check, std::move(dialog_callback),
                       std::move(callback), use_fallback));
    return;
  }
  provider_->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          std::move(install_surface), std::move(contents),
          bypass_service_worker_check, std::move(dialog_callback),
          std::move(callback), use_fallback, &provider_->install_finalizer(),
          std::make_unique<WebAppDataRetriever>()));
}

}  // namespace web_app

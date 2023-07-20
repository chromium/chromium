// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_externally_managed_app_manager.h"

#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"

namespace web_app {

FakeExternallyManagedAppManager::FakeExternallyManagedAppManager(
    Profile* profile)
    : ExternallyManagedAppManager(profile) {}

FakeExternallyManagedAppManager::~FakeExternallyManagedAppManager() = default;

void FakeExternallyManagedAppManager::InstallNow(
    ExternalInstallOptions install_options,
    OnceInstallCallback callback) {
  Install(install_options, std::move(callback));
}

void FakeExternallyManagedAppManager::Install(
    ExternalInstallOptions install_options,
    OnceInstallCallback callback) {
  install_requests_.push_back(install_options);
  if (handle_install_request_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(handle_install_request_callback_, install_options),
        base::BindOnce(std::move(callback), install_options.install_url));
    return;
  }
  ExternallyManagedAppManager::Install(install_options, std::move(callback));
}

void FakeExternallyManagedAppManager::InstallApps(
    std::vector<ExternalInstallOptions> install_options_list,
    const RepeatingInstallCallback& callback) {
  if (handle_install_request_callback_) {
    for (auto& install_options : install_options_list)
      Install(std::move(install_options), callback);
    return;
  }

  base::ranges::copy(install_options_list,
                     std::back_inserter(install_requests_));
  if (!drop_requests_for_testing_) {
    ExternallyManagedAppManager::InstallApps(install_options_list, callback);
  }
}

void FakeExternallyManagedAppManager::UninstallApps(
    std::vector<GURL> uninstall_urls,
    ExternalInstallSource install_source,
    const UninstallCallback& callback) {
  base::ranges::copy(uninstall_urls, std::back_inserter(uninstall_requests_));
  if (handle_uninstall_request_callback_) {
    for (auto& app_url : uninstall_urls) {
      base::SequencedTaskRunner::GetCurrentDefault()
          ->PostTaskAndReplyWithResult(
              FROM_HERE,
              base::BindOnce(handle_uninstall_request_callback_, app_url,
                             install_source),
              base::BindOnce(callback, app_url));
    }
    return;
  }
  ExternallyManagedAppManager::UninstallApps(uninstall_urls, install_source,
                                             callback);
}

void FakeExternallyManagedAppManager::SetHandleInstallRequestCallback(
    HandleInstallRequestCallback callback) {
  handle_install_request_callback_ = std::move(callback);
}

void FakeExternallyManagedAppManager::SetHandleUninstallRequestCallback(
    HandleUninstallRequestCallback callback) {
  handle_uninstall_request_callback_ = std::move(callback);
}

}  // namespace web_app

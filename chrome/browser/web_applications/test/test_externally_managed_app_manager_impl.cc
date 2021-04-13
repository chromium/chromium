// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_externally_managed_app_manager_impl.h"

#include <algorithm>

namespace web_app {

TestExternallyManagedAppManagerImpl::TestExternallyManagedAppManagerImpl(
    Profile* profile)
    : ExternallyManagedAppManagerImpl(profile) {}

TestExternallyManagedAppManagerImpl::~TestExternallyManagedAppManagerImpl() =
    default;

void TestExternallyManagedAppManagerImpl::Install(
    ExternalInstallOptions install_options,
    OnceInstallCallback callback) {
  if (handle_install_request_callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(handle_install_request_callback_, install_options),
        base::BindOnce(std::move(callback), install_options.install_url));
    return;
  }

  install_requests_.push_back(install_options);
  ExternallyManagedAppManagerImpl::Install(install_options,
                                           std::move(callback));
}

void TestExternallyManagedAppManagerImpl::InstallApps(
    std::vector<ExternalInstallOptions> install_options_list,
    const RepeatingInstallCallback& callback) {
  if (handle_install_request_callback_) {
    for (auto& install_options : install_options_list)
      Install(std::move(install_options), callback);
    return;
  }

  std::copy(install_options_list.begin(), install_options_list.end(),
            std::back_inserter(install_requests_));
  if (!drop_requests_for_testing_) {
    ExternallyManagedAppManagerImpl::InstallApps(install_options_list,
                                                 std::move(callback));
  }
}

void TestExternallyManagedAppManagerImpl::UninstallApps(
    std::vector<GURL> uninstall_urls,
    ExternalInstallSource install_source,
    const UninstallCallback& callback) {
  std::copy(uninstall_urls.begin(), uninstall_urls.end(),
            std::back_inserter(uninstall_requests_));
  ExternallyManagedAppManagerImpl::UninstallApps(uninstall_urls, install_source,
                                                 std::move(callback));
}

void TestExternallyManagedAppManagerImpl::SetHandleInstallRequestCallback(
    HandleInstallRequestCallback callback) {
  handle_install_request_callback_ = std::move(callback);
}

}  // namespace web_app

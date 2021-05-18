// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_externally_managed_app_manager.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "url/gurl.h"

namespace web_app {

TestExternallyManagedAppManager::TestExternallyManagedAppManager(
    TestAppRegistrar* registrar)
    : deduped_install_count_(0),
      deduped_uninstall_count_(0),
      registrar_(registrar) {
  // TODO(crbug.com/973324): Wire this up to a TestInstallFinalizer.
  SetSubsystems(registrar, nullptr, nullptr, nullptr, nullptr);
}

TestExternallyManagedAppManager::~TestExternallyManagedAppManager() = default;

void TestExternallyManagedAppManager::SimulatePreviouslyInstalledApp(
    const GURL& url,
    ExternalInstallSource install_source) {
  registrar_->AddExternalApp(TestInstallFinalizer::GetAppIdForUrl(url),
                             {url, install_source});
}

void TestExternallyManagedAppManager::SetInstallResultCode(
    InstallResultCode result_code) {
  install_result_code_ = result_code;
}

void TestExternallyManagedAppManager::InstallNow(
    ExternalInstallOptions install_options,
    OnceInstallCallback callback) {
  Install(install_options, std::move(callback));
}

void TestExternallyManagedAppManager::Install(
    ExternalInstallOptions install_options,
    OnceInstallCallback callback) {
  // TODO(nigeltao): Add error simulation when error codes are added to the API.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  auto result_code = install_result_code_;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([this, weak_ptr, install_options, result_code,
                                  callback = std::move(callback)]() mutable {
        const GURL& url = install_options.install_url;
        // Use a WeakPtr to be able to simulate the Install callback running
        // after ExternallyManagedAppManager gets deleted.
        if (weak_ptr) {
          if (!registrar_->LookupExternalAppId(url)) {
            registrar_->AddExternalApp(
                TestInstallFinalizer::GetAppIdForUrl(url),
                {url, install_options.install_source});
            deduped_install_count_++;
          }
          install_requests_.push_back(install_options);
        }
        std::move(std::move(callback)).Run(url, {.code = result_code});
      }));
}

void TestExternallyManagedAppManager::InstallApps(
    std::vector<ExternalInstallOptions> install_options_list,
    const RepeatingInstallCallback& callback) {
  for (auto& install_options : install_options_list)
    Install(std::move(install_options), callback);
}

void TestExternallyManagedAppManager::UninstallApps(
    std::vector<GURL> uninstall_urls,
    ExternalInstallSource install_source,
    const UninstallCallback& callback) {
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  for (const auto& url : uninstall_urls) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([this, weak_ptr, url, callback]() {
          if (weak_ptr) {
            absl::optional<AppId> app_id = registrar_->LookupExternalAppId(url);
            if (app_id) {
              registrar_->RemoveExternalApp(*app_id);
              deduped_uninstall_count_++;
            }
            uninstall_requests_.push_back(url);
          }
          callback.Run(url, true /* succeeded */);
        }));
  }
}

}  // namespace web_app

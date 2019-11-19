// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_PENDING_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_PENDING_APP_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "url/gurl.h"

namespace web_app {

class TestAppRegistrar;

class TestPendingAppManager : public PendingAppManager {
 public:
  explicit TestPendingAppManager(TestAppRegistrar* registrar);
  ~TestPendingAppManager() override;

  // The foo_requests methods may return duplicates, if the underlying
  // InstallApps or UninstallApps arguments do. The deduped_foo_count methods
  // only count new installs or new uninstalls.

  const std::vector<ExternalInstallOptions>& install_requests() const {
    return install_requests_;
  }
  const std::vector<GURL>& uninstall_requests() const {
    return uninstall_requests_;
  }

  int deduped_install_count() const { return deduped_install_count_; }
  int deduped_uninstall_count() const { return deduped_uninstall_count_; }

  void ResetCounts() {
    deduped_install_count_ = 0;
    deduped_uninstall_count_ = 0;
  }

  void SimulatePreviouslyInstalledApp(const GURL& url,
                                      ExternalInstallSource install_source);

  void SetInstallResultCode(InstallResultCode result_code);

  // PendingAppManager:
  void Install(ExternalInstallOptions install_options,
               OnceInstallCallback callback) override;
  void InstallApps(std::vector<ExternalInstallOptions> install_options_list,
                   const RepeatingInstallCallback& callback) override;
  void UninstallApps(std::vector<GURL> uninstall_urls,
                     ExternalInstallSource install_source,
                     const UninstallCallback& callback) override;
  void Shutdown() override {}

 private:
  void DoInstall(ExternalInstallOptions install_options,
                 OnceInstallCallback callback);
  std::vector<ExternalInstallOptions> install_requests_;
  std::vector<GURL> uninstall_requests_;

  // TODO(calamity): Remove and replace with TestAppRegistrar methods.
  int deduped_install_count_;
  int deduped_uninstall_count_;

  InstallResultCode install_result_code_ =
      InstallResultCode::kSuccessNewInstall;

  TestAppRegistrar* registrar_;

  base::WeakPtrFactory<TestPendingAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestPendingAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_PENDING_APP_MANAGER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_EXTERNALLY_MANAGED_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_EXTERNALLY_MANAGED_APP_MANAGER_H_

#include <vector>

#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "components/webapps/browser/install_result_code.h"

namespace web_app {

class FakeExternallyManagedAppManager : public ExternallyManagedAppManager {
 public:
  explicit FakeExternallyManagedAppManager(Profile* profile);
  ~FakeExternallyManagedAppManager() override;

  void InstallNow(ExternalInstallOptions install_options,
                  OnceInstallCallback callback) override;
  void Install(ExternalInstallOptions install_options,
               OnceInstallCallback callback) override;
  void InstallApps(std::vector<ExternalInstallOptions> install_options_list,
                   const RepeatingInstallCallback& callback) override;
  void UninstallApps(std::vector<GURL> uninstall_urls,
                     ExternalInstallSource install_source,
                     const UninstallCallback& callback) override;

  const std::vector<ExternalInstallOptions>& install_requests() const {
    return install_requests_;
  }
  const std::vector<GURL>& uninstall_requests() const {
    return uninstall_requests_;
  }

  void SetDropRequestsForTesting(bool drop_requests_for_testing) {
    drop_requests_for_testing_ = drop_requests_for_testing;
  }

  using HandleInstallRequestCallback =
      base::RepeatingCallback<ExternallyManagedAppManager::InstallResult(
          const ExternalInstallOptions&)>;

  using HandleUninstallRequestCallback = base::RepeatingCallback<
      webapps::UninstallResultCode(const GURL&, ExternalInstallSource)>;

  // Set a callback to handle install requests. If set, this callback will be
  // used in place of the real installation process. The callback takes a const
  // ExternalInstallOptions& and should return a webapps::InstallResultCode.
  void SetHandleInstallRequestCallback(HandleInstallRequestCallback callback);

  // Set a callback to handle uninstall requests. If set, this callback will be
  // used in place of the real uninstallation process. The callback takes a
  // const ExternalInstallOptions& and should return a bool.
  void SetHandleUninstallRequestCallback(
      HandleUninstallRequestCallback callback);

 private:
  std::vector<ExternalInstallOptions> install_requests_;
  std::vector<GURL> uninstall_requests_;
  bool drop_requests_for_testing_ = false;
  HandleInstallRequestCallback handle_install_request_callback_;
  HandleUninstallRequestCallback handle_uninstall_request_callback_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_EXTERNALLY_MANAGED_APP_MANAGER_H_

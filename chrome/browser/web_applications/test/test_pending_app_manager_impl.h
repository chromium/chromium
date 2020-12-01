// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_PENDING_APP_MANAGER_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_PENDING_APP_MANAGER_IMPL_H_

#include <vector>

#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/pending_app_manager_impl.h"

namespace web_app {

class TestPendingAppManagerImpl : public PendingAppManagerImpl {
 public:
  explicit TestPendingAppManagerImpl(Profile* profile);
  ~TestPendingAppManagerImpl() override;

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
      base::RepeatingCallback<PendingAppManager::InstallResult(
          const ExternalInstallOptions&)>;

  // Set a callback to handle install requests. If set, this callback will be
  // used in place of the real installation process. The callback takes a const
  // ExternalInstallOptions& and should return a InstallResultCode.
  void SetHandleInstallRequestCallback(HandleInstallRequestCallback callback);

 private:
  std::vector<ExternalInstallOptions> install_requests_;
  std::vector<GURL> uninstall_requests_;
  bool drop_requests_for_testing_ = false;
  HandleInstallRequestCallback handle_install_request_callback_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_PENDING_APP_MANAGER_IMPL_H_

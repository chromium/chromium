// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_TEST_IWA_INSTALLER_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_TEST_IWA_INSTALLER_FACTORY_H_

#include <map>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/mock_isolated_web_app_install_command_wrapper.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

class MockIsolatedWebAppInstallCommandWrapper;

class TestIwaInstallerFactory {
 public:
  TestIwaInstallerFactory();
  TestIwaInstallerFactory(const TestIwaInstallerFactory&) = delete;
  TestIwaInstallerFactory& operator=(const TestIwaInstallerFactory&) = delete;
  ~TestIwaInstallerFactory();

  void SetUp(Profile* profile);

  void SetCommandBehavior(
      const webapps::AppId& app_id,
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode execution_mode,
      bool execute_immediately);

  MockIsolatedWebAppInstallCommandWrapper* GetLatestCommandWrapper(
      const webapps::AppId& app_id);

  void SetInstallCompletedClosure(base::RepeatingClosure closure);

  size_t GetNumberOfCreatedInstallTasks() const {
    return number_of_install_tasks_created_;
  }

 private:
  std::unique_ptr<IwaInstaller> CreateIwaInstaller(
      Profile* profile,
      IsolatedWebAppExternalInstallOptions install_options,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::Value::List& log,
      WebAppProvider* provider,
      IwaInstaller::ResultCallback callback);

  // Maps app id to whether the command should
  // * succeed (true) or fail (false).
  // * be executed immediately (true) or requires a manual trigger from the
  // test (false).
  std::map<
      webapps::AppId,
      std::pair<MockIsolatedWebAppInstallCommandWrapper::ExecutionMode, bool>>
      command_behaviors_;
  std::map<webapps::AppId, raw_ptr<MockIsolatedWebAppInstallCommandWrapper>>
      latest_install_wrappers_;
  size_t number_of_install_tasks_created_ = 0U;
  base::RepeatingClosure closure_ = base::DoNothing();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_TEST_IWA_INSTALLER_FACTORY_H_

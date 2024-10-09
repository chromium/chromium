// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/test_iwa_installer_factory.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/mock_isolated_web_app_install_command_wrapper.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/common/web_app_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace web_app {

TestIwaInstallerFactory::TestIwaInstallerFactory() = default;

TestIwaInstallerFactory::~TestIwaInstallerFactory() = default;

void TestIwaInstallerFactory::SetUp(Profile* profile) {
  IwaInstallerFactory::GetIwaInstallerFactory() =
      base::BindRepeating(&TestIwaInstallerFactory::CreateIwaInstaller,
                          base::Unretained(this), profile);
}

void TestIwaInstallerFactory::SetCommandBehavior(
    const webapps::AppId& app_id,
    MockIsolatedWebAppInstallCommandWrapper::ExecutionMode execution_mode,
    bool execute_immediately) {
  command_behaviors_[app_id] = {execution_mode, execute_immediately};
}

MockIsolatedWebAppInstallCommandWrapper*
TestIwaInstallerFactory::GetLatestCommandWrapper(const webapps::AppId& app_id) {
  if (latest_install_wrappers_.contains(app_id)) {
    return latest_install_wrappers_[app_id];
  }
  return nullptr;
}

void TestIwaInstallerFactory::SetInstallCompletedClosure(
    base::RepeatingClosure closure) {
  closure_ = std::move(closure);
}

std::unique_ptr<IwaInstaller> TestIwaInstallerFactory::CreateIwaInstaller(
    Profile* profile,
    IsolatedWebAppExternalInstallOptions install_options,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::Value::List& log,
    WebAppProvider* provider,
    IwaInstaller::ResultCallback callback) {
  CHECK(command_behaviors_.contains(install_options.web_bundle_id().id()));
  const webapps::AppId& app_id = install_options.web_bundle_id().id();
  auto& command_behavior = command_behaviors_[app_id];
  ++number_of_install_tasks_created_;
  auto install_command_wrapper =
      std::make_unique<MockIsolatedWebAppInstallCommandWrapper>(
          profile, provider, command_behavior.first, command_behavior.second);
  latest_install_wrappers_[app_id] = install_command_wrapper.get();
  return std::make_unique<IwaInstaller>(
      std::move(install_options), std::move(url_loader_factory),
      std::move(install_command_wrapper), log,
      base::BindOnce(
          [](TestIwaInstallerFactory* installer_instance,
             IwaInstaller::ResultCallback callback, webapps::AppId app_id,
             IwaInstaller::Result result) {
            installer_instance->latest_install_wrappers_.erase(app_id);
            std::move(callback).Run(result);
          },
          base::Unretained(this), std::move(callback), app_id)
          .Then(closure_));
}

}  // namespace web_app

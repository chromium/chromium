// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: rename this file?
#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_MOCK_ISOLATED_WEB_APP_INSTALL_COMMAND_WRAPPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_MOCK_ISOLATED_WEB_APP_INSTALL_COMMAND_WRAPPER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"

class Profile;

namespace base {
class Version;
}  // namespace base

namespace web_app {

class IsolatedWebAppInstallSource;
class IsolatedWebAppUrlInfo;
class WebAppProvider;

// This class lets tests simulate installation behavior. A test can control
// the execution mode (run the production install command; simulate success;
// simulate a failure) and the schedule (schedule command immediately; use the
// `ScheduleCommand` function to explicitly trigger scheduling of the command).
class MockIsolatedWebAppInstallCommandWrapper
    : public IwaInstaller::IwaInstallCommandWrapper {
 public:
  enum class ExecutionMode { kRunCommand, kSimulateSuccess, kSimulateFailure };

  MockIsolatedWebAppInstallCommandWrapper(Profile* profile,
                                          WebAppProvider* provider,
                                          ExecutionMode execution_mode,
                                          bool schedule_command_immediately);
  MockIsolatedWebAppInstallCommandWrapper(
      const MockIsolatedWebAppInstallCommandWrapper&) = delete;
  MockIsolatedWebAppInstallCommandWrapper& operator=(
      const MockIsolatedWebAppInstallCommandWrapper&) = delete;
  ~MockIsolatedWebAppInstallCommandWrapper() override;

  void Install(
      const IsolatedWebAppInstallSource& install_source,
      const IsolatedWebAppUrlInfo& url_info,
      const base::Version& expected_version,
      WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) override;

  void ScheduleCommand();

  bool CommandWasScheduled() const { return command_was_scheduled_; }

 private:
  const raw_ptr<web_app::WebAppProvider> provider_;
  const raw_ptr<Profile> profile_;
  ExecutionMode execution_mode_;
  const bool schedule_command_immediately_;

  bool command_was_scheduled_ = false;
  std::optional<IsolatedWebAppInstallSource> install_source_;
  std::optional<IsolatedWebAppUrlInfo> url_info_;
  std::optional<base::Version> expected_version_;
  std::optional<WebAppCommandScheduler::InstallIsolatedWebAppCallback>
      callback_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_MOCK_ISOLATED_WEB_APP_INSTALL_COMMAND_WRAPPER_H_

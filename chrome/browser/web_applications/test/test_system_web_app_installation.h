// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_

#include <memory>
#include <string>

#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_system_web_app_web_ui_controller_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"

namespace web_app {

// Class to setup the installation of a test System Web App.
//
// Use SetUp*() methods to create a instance of this class in test suite's
// constructor, before the profile is fully created. In tests, call
// WaitForAppInstall() to finish the installation.
class TestSystemWebAppInstallation {
 public:
  enum IncludeLaunchDirectory { kYes, kNo };

  // Used for tests that don't want to install any System Web Apps.
  static std::unique_ptr<TestSystemWebAppInstallation> SetUpWithoutApps();

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpTabbedMultiWindowApp(const bool use_web_app_info);

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpStandaloneSingleWindowApp(const bool use_web_app_info);

  // This method automatically grants Native File System read and write
  // permissions to the App.
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppThatReceivesLaunchFiles(
      IncludeLaunchDirectory include_launch_directory,
      const bool use_web_app_info);

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithEnabledOriginTrials(const OriginTrialsMap& origin_to_trials,
                                  const bool use_web_app_info);

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppNotShownInLauncher(const bool use_web_app_info);

  static std::unique_ptr<TestSystemWebAppInstallation> SetUpAppNotShownInSearch(
      const bool use_web_app_info);

  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppWithAdditionalSearchTerms(const bool use_web_app_info);

  // This method additionally sets up a helper SystemAppType::SETTING system app
  // for testing capturing links from a different SWA.
  static std::unique_ptr<TestSystemWebAppInstallation>
  SetUpAppThatCapturesNavigation(const bool use_web_app_info);

  static std::unique_ptr<TestSystemWebAppInstallation> SetUpChromeUntrustedApp(
      const bool use_web_app_info);

  ~TestSystemWebAppInstallation();

  void WaitForAppInstall();

  AppId GetAppId();
  const GURL& GetAppUrl();
  SystemAppType GetType();

  // Override the contents served by chrome://test-system-app/manifest.json.
  void SetManifest(std::string manifest);

  void set_update_policy(SystemWebAppManager::UpdatePolicy update_policy) {
    update_policy_ = update_policy;
  }

 private:
  TestSystemWebAppInstallation(SystemAppType type, SystemAppInfo info);
  TestSystemWebAppInstallation();

  std::unique_ptr<KeyedService> CreateWebAppProvider(SystemAppInfo info,
                                                     Profile* profile);
  std::unique_ptr<KeyedService> CreateWebAppProviderWithNoSystemWebApps(
      Profile* profile);

  // Must be called in SetUp*App() methods, before WebAppProvider is created.
  void RegisterAutoGrantedPermissions(ContentSettingsType permission);

  Profile* profile_;
  SystemWebAppManager::UpdatePolicy update_policy_ =
      SystemWebAppManager::UpdatePolicy::kAlwaysUpdate;
  std::unique_ptr<TestWebAppProviderCreator> test_web_app_provider_creator_;
  // nullopt if SetUpWithoutApps() was used.
  const base::Optional<SystemAppType> type_;
  std::vector<std::unique_ptr<TestSystemWebAppWebUIControllerFactory>>
      web_ui_controller_factories_;
  std::set<ContentSettingsType> auto_granted_permissions_;
  base::flat_map<SystemAppType, SystemAppInfo> extra_apps_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_INSTALLATION_H_

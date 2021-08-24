// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_REGISTRY_CONTROLLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_REGISTRY_CONTROLLER_H_

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

class TestWebAppDatabaseFactory;
class WebAppRegistrarMutable;

// TODO(crbug.com/1225132): Retire TestAppRegistryController.
class TestAppRegistryController : public WebAppSyncBridge {
 public:
  explicit TestAppRegistryController(Profile* profile);

  TestAppRegistryController(
      Profile* profile,
      std::unique_ptr<TestWebAppDatabaseFactory> database_factory,
      std::unique_ptr<WebAppRegistrarMutable> registrar);

  ~TestAppRegistryController() override;

  // WebAppSyncBridge:
  void Init(base::OnceClosure callback) override;
  void SetAppUserDisplayMode(const AppId& app_id,
                             DisplayMode display_mode,
                             bool is_user_action) override;
  void SetAppIsDisabled(const AppId& app_id, bool is_disabled) override;
  void UpdateAppsDisableMode() override;
  void SetExperimentalTabbedWindowMode(const AppId& app_id,
                                       bool enabled,
                                       bool is_user_action) override;
  void SetAppIsLocallyInstalled(const AppId& app_id,
                                bool is_locally_installed) override;
  void SetAppLastBadgingTime(const AppId& app_id,
                             const base::Time& time) override;
  void SetAppLastLaunchTime(const AppId& app_id,
                            const base::Time& time) override;
  void SetAppInstallTime(const AppId& app_id, const base::Time& time) override;
  void SetAppRunOnOsLoginMode(const AppId& app_id,
                              RunOnOsLoginMode mode) override;
  void SetAppWindowControlsOverlayEnabled(const AppId& app_id,
                                          bool enabled) override;

  WebAppSyncBridge* AsWebAppSyncBridge() override;

 private:
  std::unique_ptr<TestWebAppDatabaseFactory> database_factory_;
  std::unique_ptr<WebAppRegistrarMutable> registrar_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_REGISTRY_CONTROLLER_H_

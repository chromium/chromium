// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_MANAGER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/version.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

class TestSystemWebAppManager : public SystemWebAppManager {
 public:
  explicit TestSystemWebAppManager(Profile* profile);
  ~TestSystemWebAppManager() override;

  void SetSystemApps(base::flat_map<SystemAppType, SystemAppInfo> system_apps);

  void SetUpdatePolicy(SystemWebAppManager::UpdatePolicy policy);

  void set_current_version(const base::Version& version) {
    current_version_ = version;
  }

  void set_current_locale(const std::string& locale) {
    current_locale_ = locale;
  }

  // SystemWebAppManager:
  const base::Version& CurrentVersion() const override;
  const std::string& CurrentLocale() const override;

 private:
  base::Version current_version_{"0.0.0.0"};
  std::string current_locale_;

  DISALLOW_COPY_AND_ASSIGN(TestSystemWebAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_MANAGER_H_

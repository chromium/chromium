// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/locks/full_system_lock.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace web_app {

// This class runs web apps on OS Login on ChromeOS once the corresponding
// policy has been read by the WebAppPolicyManager.
class WebAppRunOnOsLoginManager {
 public:
  explicit WebAppRunOnOsLoginManager(WebAppCommandScheduler* scheduler);
  WebAppRunOnOsLoginManager(const WebAppRunOnOsLoginManager&) = delete;
  WebAppRunOnOsLoginManager& operator=(const WebAppRunOnOsLoginManager&) =
      delete;
  ~WebAppRunOnOsLoginManager();

  void Start();

  base::WeakPtr<WebAppRunOnOsLoginManager> GetWeakPtr();

  void SetSkipStartupForTesting(bool skip_startup);
  void RunAppsOnOsLoginForTesting();

 private:
  void RunAppsOnOsLogin(FullSystemLock& lock);
  void OnAppLaunchedOnOsLogin(AppId app_id,
                              std::string app_name,
                              Browser* browser,
                              content::WebContents* web_contents,
                              apps::LaunchContainer container);

  raw_ref<WebAppCommandScheduler, DanglingUntriaged> scheduler_;

  bool skip_startup_for_testing_ = false;

  base::WeakPtrFactory<WebAppRunOnOsLoginManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_RUN_ON_OS_LOGIN_MANAGER_H_

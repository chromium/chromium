// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_RUN_ON_OS_LOGIN_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace web_app {

class AllAppsLock;
class WebAppProvider;

// This class runs web apps on OS Login on ChromeOS once the corresponding
// policy has been read by the WebAppPolicyManager.
class WebAppRunOnOsLoginManager {
 public:
  explicit WebAppRunOnOsLoginManager(Profile* profile);
  WebAppRunOnOsLoginManager(const WebAppRunOnOsLoginManager&) = delete;
  WebAppRunOnOsLoginManager& operator=(const WebAppRunOnOsLoginManager&) =
      delete;
  ~WebAppRunOnOsLoginManager();

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  void Start();

  base::WeakPtr<WebAppRunOnOsLoginManager> GetWeakPtr();

  static base::AutoReset<bool> SkipStartupForTesting();
  void RunAppsOnOsLoginForTesting();

 private:
  void RunAppsOnOsLogin(AllAppsLock& lock);

  void ShowAppLaunchedNotification(const std::vector<std::string>& app_names);

  raw_ptr<WebAppProvider> provider_ = nullptr;
  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<WebAppRunOnOsLoginManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_RUN_ON_OS_LOGIN_MANAGER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_POLICY_WEB_APP_POLICY_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_POLICY_WEB_APP_POLICY_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/gurl.h"

class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

// Tracks the policy that affects Web Apps and also tracks which Web Apps are
// currently installed based on this policy. Based on these, it decides which
// apps to install, uninstall, and update, via a PendingAppManager.
class WebAppPolicyManager {
 public:
  // Constructs a WebAppPolicyManager instance that uses
  // |pending_app_manager| to manage apps. |pending_app_manager| should outlive
  // this class.
  WebAppPolicyManager(Profile* profile, PendingAppManager* pending_app_manager);
  ~WebAppPolicyManager();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static bool ShouldEnableForProfile(Profile* profile);

 private:
  void InitChangeRegistrarAndRefreshPolicyInstalledApps();

  void RefreshPolicyInstalledApps();

  Profile* profile_;
  PrefService* pref_service_;

  // Used to install, uninstall, and update apps. Should outlive this class.
  PendingAppManager* pending_app_manager_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<WebAppPolicyManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppPolicyManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_POLICY_WEB_APP_POLICY_MANAGER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_SYSTEM_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_SYSTEM_WEB_APP_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

// Installs, uninstalls, and updates System Web Apps.
class SystemWebAppManager {
 public:
  // Constructs a SystemWebAppManager instance that uses
  // |pending_app_manager| to manage apps. |pending_app_manager| should outlive
  // this class.
  SystemWebAppManager(Profile* profile, PendingAppManager* pending_app_manager);
  virtual ~SystemWebAppManager();

  static bool ShouldEnableForProfile(Profile* profile);

 protected:
  // Overridden in tests.
  virtual std::vector<GURL> CreateSystemWebApps();

 private:
  void StartAppInstallation();

  Profile* profile_;

  // Used to install, uninstall, and update apps. Should outlive this class.
  PendingAppManager* pending_app_manager_;

  base::WeakPtrFactory<SystemWebAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_SYSTEM_WEB_APP_MANAGER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SHORTCUT_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"

class Profile;

namespace web_app {

class WebAppShortcutManager : public AppShortcutManager {
 public:
  explicit WebAppShortcutManager(Profile* profile);
  ~WebAppShortcutManager() override;

  bool CanCreateShortcuts() const override;

  void GetShortcutInfoForApp(const AppId& app_id,
                             GetShortcutInfoCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppShortcutManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SHORTCUT_MANAGER_H_

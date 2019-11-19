// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_SHORTCUT_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"

class Profile;

namespace extensions {

class BookmarkAppShortcutManager : public web_app::AppShortcutManager {
 public:
  explicit BookmarkAppShortcutManager(Profile* profile);
  ~BookmarkAppShortcutManager() override;

  void GetShortcutInfoForApp(const web_app::AppId& app_id,
                             GetShortcutInfoCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppShortcutManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_SHORTCUT_MANAGER_H_

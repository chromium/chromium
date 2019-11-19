// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_ICON_MANAGER_H_

#include "chrome/browser/web_applications/components/app_icon_manager.h"

class Profile;

namespace extensions {

// Class used to read icons of extensions-based bookmark apps.
// TODO(crbug.com/877898): Erase this subclass once BookmarkApps are off
// Extensions.
class BookmarkAppIconManager : public web_app::AppIconManager {
 public:
  explicit BookmarkAppIconManager(Profile* profile);
  ~BookmarkAppIconManager() override;

  // AppIconManager:
  bool ReadIcon(const web_app::AppId& app_id,
                int icon_size_in_px,
                ReadIconCallback callback) override;
  bool ReadSmallestIcon(const web_app::AppId& app_id,
                        int icon_size_in_px,
                        ReadIconCallback callback) override;

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppIconManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_ICON_MANAGER_H_

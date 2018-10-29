// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_BOOKMARK_APP_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_BOOKMARK_APP_INSTALL_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/web_applications/components/install_manager.h"

namespace extensions {

class BookmarkAppInstallManager final : public web_app::InstallManager {
 public:
  BookmarkAppInstallManager();
  ~BookmarkAppInstallManager() override;

  // InstallManager interface implementation.
  bool CanInstallWebApp(const content::WebContents* web_contents) override;
  void InstallWebApp(content::WebContents* web_contents,
                     bool force_shortcut_app,
                     OnceInstallCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_BOOKMARK_APP_INSTALL_MANAGER_H_

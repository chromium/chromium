// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MANIFEST_WEB_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_MANIFEST_WEB_APP_BROWSER_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkColor.h"

class Browser;

namespace gfx {
class ImageSkia;
}

// Class to encapsulate logic to control the browser UI for manifest based web
// apps or focus mode.
class ManifestWebAppBrowserController : public web_app::AppBrowserController {
 public:
  explicit ManifestWebAppBrowserController(Browser* browser);
  ~ManifestWebAppBrowserController() override;

  // web_app::AppBrowserController:
  bool HasMinimalUiButtons() const override;
  bool ShouldShowCustomTabBar() const override;
  gfx::ImageSkia GetWindowAppIcon() const override;
  gfx::ImageSkia GetWindowIcon() const override;
  std::string GetAppShortName() const override;
  base::string16 GetFormattedUrlOrigin() const override;
  GURL GetAppLaunchURL() const override;
  bool IsUrlInAppScope(const GURL& url) const override;

 protected:
  // web_app::AppBrowserController:
  void OnTabInserted(content::WebContents* contents) override;

 private:
  GURL app_launch_url_;
};

#endif  // CHROME_BROWSER_UI_MANIFEST_WEB_APP_BROWSER_CONTROLLER_H_

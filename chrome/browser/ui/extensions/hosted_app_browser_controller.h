// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "third_party/skia/include/core/SkColor.h"

class Browser;

namespace gfx {
class ImageSkia;
}

namespace extensions {

class Extension;

// Class to encapsulate logic to control the browser UI for extension based web
// apps.
class HostedAppBrowserController : public web_app::AppBrowserController {
 public:
  // Functions to set preferences that are unique to app windows.
  static void SetAppPrefsForWebContents(
      web_app::AppBrowserController* controller,
      content::WebContents* web_contents);

  // Clear preferences that are unique to app windows.
  static void ClearAppPrefsForWebContents(content::WebContents* web_contents);

  explicit HostedAppBrowserController(Browser* browser);
  ~HostedAppBrowserController() override;

  // web_app::AppBrowserController:
  bool CreatedForInstalledPwa() const override;
  bool HasMinimalUiButtons() const override;
  gfx::ImageSkia GetWindowAppIcon() const override;
  gfx::ImageSkia GetWindowIcon() const override;
  base::Optional<SkColor> GetThemeColor() const override;
  base::string16 GetTitle() const override;
  std::string GetAppShortName() const override;
  base::string16 GetFormattedUrlOrigin() const override;
  GURL GetAppLaunchURL() const override;
  bool IsUrlInAppScope(const GURL& url) const override;
  const Extension* GetExtensionForTesting() const;
  bool CanUninstall() const override;
  void Uninstall() override;
  bool IsInstalled() const override;
  bool IsHostedApp() const override;

 protected:
  // web_app::AppBrowserController:
  void OnReceivedInitialURL() override;
  void OnTabInserted(content::WebContents* contents) override;
  void OnTabRemoved(content::WebContents* contents) override;

 private:
  // Will return nullptr if the extension has been uninstalled.
  const Extension* GetExtension() const;

  const bool created_for_installed_pwa_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppBrowserController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

class Browser;
class SkBitmap;

namespace web_app {

class AppRegistrar;
class WebAppProvider;

// Class to encapsulate logic to control the browser UI for
// web apps.
// App information is obtained from the AppRegistrar.
// Icon information is obtained from the AppIconManager.
// Note: Much of the functionality in HostedAppBrowserController
// will move to this class.
class WebAppBrowserController : public AppBrowserController {
 public:
  explicit WebAppBrowserController(Browser* browser);
  ~WebAppBrowserController() override;

  // AppBrowserController:
  bool CreatedForInstalledPwa() const override;
  bool HasMinimalUiButtons() const override;
  gfx::ImageSkia GetWindowAppIcon() const override;
  gfx::ImageSkia GetWindowIcon() const override;
  base::Optional<SkColor> GetThemeColor() const override;
  std::string GetAppShortName() const override;
  base::string16 GetFormattedUrlOrigin() const override;
  GURL GetAppLaunchURL() const override;
  bool IsUrlInAppScope(const GURL& url) const override;
  WebAppBrowserController* AsWebAppBrowserController() override;
  bool CanUninstall() const override;
  void Uninstall() override;
  bool IsInstalled() const override;
  bool IsHostedApp() const override;

  void SetReadIconCallbackForTesting(base::OnceClosure callback);

 private:
  const AppRegistrar& registrar() const;

  void OnReadIcon(SkBitmap bitmap);

  WebAppProvider& provider_;
  mutable base::Optional<gfx::ImageSkia> app_icon_;

  base::OnceClosure callback_for_testing_;
  mutable base::WeakPtrFactory<WebAppBrowserController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppBrowserController);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_

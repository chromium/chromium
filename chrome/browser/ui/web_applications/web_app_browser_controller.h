// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/installable/digital_asset_links/digital_asset_links_handler.h"
#endif

class Browser;
class SkBitmap;

namespace digital_asset_links {
class DigitalAssetLinksHandler;
}

namespace web_app {

class AppRegistrar;
class WebAppProvider;

// Class to encapsulate logic to control the browser UI for
// web apps.
// App information is obtained from the AppRegistrar.
// Icon information is obtained from the AppIconManager.
// Note: Much of the functionality in HostedAppBrowserController
// will move to this class.
class WebAppBrowserController : public AppBrowserController,
                                public AppRegistrarObserver {
 public:
  explicit WebAppBrowserController(Browser* browser);
  ~WebAppBrowserController() override;

  // AppBrowserController:
  bool HasMinimalUiButtons() const override;
  gfx::ImageSkia GetWindowAppIcon() const override;
  gfx::ImageSkia GetWindowIcon() const override;
  base::Optional<SkColor> GetThemeColor() const override;
  base::Optional<SkColor> GetBackgroundColor() const override;
  base::string16 GetTitle() const override;
  base::string16 GetAppShortName() const override;
  base::string16 GetFormattedUrlOrigin() const override;
  GURL GetAppStartUrl() const override;
  bool IsUrlInAppScope(const GURL& url) const override;
  WebAppBrowserController* AsWebAppBrowserController() override;
  bool CanUninstall() const override;
  void Uninstall() override;
  bool IsInstalled() const override;
  bool IsHostedApp() const override;

#if defined(OS_CHROMEOS)
  bool ShouldShowCustomTabBar() const override;
#endif  // OS_CHROMEOS

  // AppRegistrarObserver:
  void OnWebAppUninstalled(const AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  void SetReadIconCallbackForTesting(base::OnceClosure callback);

 protected:
  // web_app::AppBrowserController:
  void OnTabInserted(content::WebContents* contents) override;
  void OnTabRemoved(content::WebContents* contents) override;

 private:
  const AppRegistrar& registrar() const;

  // Helper function to call AppServiceProxy to load icon.
  void LoadAppIcon(bool allow_placeholder_icon) const;
  // Invoked when the icon is loaded.
  void OnLoadIcon(apps::mojom::IconValuePtr icon_value);

  void OnReadIcon(const SkBitmap& bitmap);
  void PerformDigitalAssetLinkVerification(Browser* browser);

#if defined(OS_CHROMEOS)
  void OnRelationshipCheckComplete(
      digital_asset_links::RelationshipCheckResult result);
#endif  // OS_CHROMEOS

  WebAppProvider& provider_;
  mutable base::Optional<gfx::ImageSkia> app_icon_;

#if defined(OS_CHROMEOS)
  // The result of digital asset link verification of the web app.
  // Only used for web-only TWAs installed through the Play Store.
  base::Optional<bool> is_verified_;

  std::unique_ptr<digital_asset_links::DigitalAssetLinksHandler>
      asset_link_handler_;
#endif  // OS_CHROMEOS

  ScopedObserver<AppRegistrar, AppRegistrarObserver> registrar_observer_{this};

  base::OnceClosure callback_for_testing_;
  mutable base::WeakPtrFactory<WebAppBrowserController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppBrowserController);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_

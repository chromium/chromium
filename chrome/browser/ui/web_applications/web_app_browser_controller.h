// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/liburlpattern/options.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/re2/src/re2/set.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/content_relationship_verification/digital_asset_links_handler.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/web_app_service.mojom-forward.h"
#endif

class Browser;
class SkBitmap;

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
class SystemWebAppDelegate;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace content_relationship_verification {
class DigitalAssetLinksHandler;
}

namespace web_app {

class WebAppRegistrar;
class WebAppProvider;

// Class to encapsulate logic to control the browser UI for
// web apps.
// App information is obtained from the WebAppRegistrar.
// Icon information is obtained from the WebAppIconManager.
// Note: Much of the functionality in HostedAppBrowserController
// will move to this class.
class WebAppBrowserController : public AppBrowserController,
                                public WebAppInstallManagerObserver {
 public:
  WebAppBrowserController(WebAppProvider& provider,
                          Browser* browser,
                          AppId app_id,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                          const ash::SystemWebAppDelegate* system_app,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
                          bool has_tab_strip);
  WebAppBrowserController(const WebAppBrowserController&) = delete;
  WebAppBrowserController& operator=(const WebAppBrowserController&) = delete;
  ~WebAppBrowserController() override;

  // AppBrowserController:
  using HomeTabCallbackList = base::OnceCallbackList<void()>;
  bool HasMinimalUiButtons() const override;
  gfx::ImageSkia GetHomeTabIcon() const;
  gfx::ImageSkia GetFallbackHomeTabIcon() const;
  ui::ImageModel GetWindowAppIcon() const override;
  ui::ImageModel GetWindowIcon() const override;
  absl::optional<SkColor> GetThemeColor() const override;
  absl::optional<SkColor> GetBackgroundColor() const override;
  std::u16string GetTitle() const override;
  std::u16string GetAppShortName() const override;
  std::u16string GetFormattedUrlOrigin() const override;
  GURL GetAppStartUrl() const override;
  GURL GetAppNewTabUrl() const override;
  bool ShouldHideNewTabButton() const override;
  bool IsUrlInHomeTabScope(const GURL& url) const override;
  bool ShouldShowAppIconOnTab(int index) const override;
  bool IsUrlInAppScope(const GURL& url) const override;
  WebAppBrowserController* AsWebAppBrowserController() override;
  bool CanUserUninstall() const override;
  void Uninstall(
      webapps::WebappUninstallSource webapp_uninstall_source) override;
  bool IsInstalled() const override;
  bool IsHostedApp() const override;
  std::unique_ptr<TabMenuModelFactory> GetTabMenuModelFactory() const override;
  bool AppUsesWindowControlsOverlay() const override;
  bool AppUsesTabbed() const override;
  bool IsWindowControlsOverlayEnabled() const override;
  void ToggleWindowControlsOverlayEnabled(
      base::OnceClosure on_complete) override;
  bool AppUsesBorderlessMode() const override;
  bool IsIsolatedWebApp() const override;
  void SetIsolatedWebAppTrueForTesting() override;
  gfx::Rect GetDefaultBounds() const override;
  bool HasReloadButton() const override;
#if !BUILDFLAG(IS_CHROMEOS)
  bool HasProfileMenuButton() const override;
#endif  // !BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const ash::SystemWebAppDelegate* system_app() const override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
  bool ShouldShowCustomTabBar() const override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
  bool AlwaysShowToolbarInFullscreen() const override;
  void ToggleAlwaysShowToolbarInFullscreen() override;
#endif

  // WebAppInstallManagerObserver:
  void OnWebAppUninstalled(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;
  void OnWebAppManifestUpdated(const AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  base::CallbackListSubscription AddHomeTabIconLoadCallbackForTesting(
      const base::OnceClosure callback);
  static void SetIconLoadCallbackForTesting(base::OnceClosure callback);
  static void SetManifestUpdateAppliedCallbackForTesting(
      base::OnceClosure callback);

 protected:
  // AppBrowserController:
  void OnTabInserted(content::WebContents* contents) override;
  void OnTabRemoved(content::WebContents* contents) override;

 private:
  mutable HomeTabCallbackList home_tab_callback_list_;
  const WebAppRegistrar& registrar() const;
  const WebAppInstallManager& install_manager() const;

  // Helper function to call AppServiceProxy to load icon.
  void LoadAppIcon(bool allow_placeholder_icon) const;

  // Invoked when the icon is loaded.
  void OnLoadIcon(apps::IconValuePtr icon_value);

  void OnReadHomeTabIcon(SkBitmap home_tab_icon_bitmap) const;
  void OnReadIcon(IconPurpose purpose, SkBitmap bitmap);
  void PerformDigitalAssetLinkVerification(Browser* browser);

#if BUILDFLAG(IS_CHROMEOS)
  void CheckDigitalAssetLinkRelationshipForAndroidApp(
      const std::string& package_name,
      const std::string& fingerprint);
  void OnRelationshipCheckComplete(
      content_relationship_verification::RelationshipCheckResult result);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnGetAssociatedAndroidPackage(crosapi::mojom::WebAppAndroidPackagePtr);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Helper function to return the resolved background color from the manifest
  // given the current state of dark/light mode.
  absl::optional<SkColor> GetResolvedManifestBackgroundColor() const;

  // Returns the set of scope patterns for the home tab scope of tabbed web
  // apps.
  absl::optional<RE2::Set> GetTabbedHomeTabScope() const;

  const raw_ref<WebAppProvider> provider_;

  // Save the display mode at time of launch. The web app display mode may
  // change with manifest updates but the app window should continue using
  // whatever it was launched with.
  DisplayMode manifest_display_mode_ = DisplayMode::kUndefined;
  DisplayMode effective_display_mode_ = DisplayMode::kUndefined;
  bool is_isolated_web_app_for_testing_ = false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  raw_ptr<const ash::SystemWebAppDelegate> system_app_ = nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  mutable absl::optional<ui::ImageModel> app_icon_;

  mutable absl::optional<RE2::Set> home_tab_scope_;

#if BUILDFLAG(IS_CHROMEOS)
  // The result of digital asset link verification of the web app.
  // Only used for web-only TWAs installed through the Play Store.
  absl::optional<bool> is_verified_;

  std::unique_ptr<content_relationship_verification::DigitalAssetLinksHandler>
      asset_link_handler_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  mutable base::WeakPtrFactory<WebAppBrowserController> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_

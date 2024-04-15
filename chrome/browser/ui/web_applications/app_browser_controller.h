// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_APP_BROWSER_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/url_formatter/url_formatter.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "url/gurl.h"

class Browser;
class BrowserThemePack;
class CustomThemeSupplier;
class TabMenuModelFactory;

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
class SystemWebAppDelegate;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
class ImageModel;
}

namespace web_app {

class WebAppBrowserController;

// Returns true if |app_url| and |page_url| are the same origin. To avoid
// breaking Hosted Apps and Bookmark Apps that might redirect to sites in the
// same domain but with "www.", this returns true if |page_url| is secure and in
// the same origin as |app_url| with "www.".
bool IsSameHostAndPort(const GURL& app_url, const GURL& page_url);

// Class to encapsulate logic to control the browser UI for web apps.
class AppBrowserController : public ui::ColorProviderKey::InitializerSupplier,
                             public TabStripModelObserver,
                             public content::WebContentsObserver,
                             public BrowserThemeProviderDelegate {
 public:
  AppBrowserController(const AppBrowserController&) = delete;
  AppBrowserController& operator=(const AppBrowserController&) = delete;
  ~AppBrowserController() override;

  // Returns whether |browser| is a web app window/pop-up.
  static bool IsWebApp(const Browser* browser);
  // Returns whether |browser| is a web app window/pop-up for |app_id|.
  static bool IsForWebApp(const Browser* browser, const webapps::AppId& app_id);
  // Returns a Browser* that is for |app_id| and |profile| if any, searches in
  // order of last browser activation. Ignores pop-up Browsers.
  static Browser* FindForWebApp(const Profile& profile,
                                const webapps::AppId& app_id);

  // Renders |url|'s origin as Unicode.
  static std::u16string FormatUrlOrigin(
      const GURL& url,
      url_formatter::FormatUrlTypes format_types =
          url_formatter::kFormatUrlOmitUsernamePassword |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitHTTP |
          url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname |
          url_formatter::kFormatUrlOmitTrivialSubdomains);

  // Initialise, must be called after construction (requires virtual dispatch).
  void Init();

  // Returns a theme built from the current page or app's theme color.
  const ui::ThemeProvider* GetThemeProvider() const;

  // Returns the text to flash in the title bar on app launch.
  std::u16string GetLaunchFlashText() const;

  // Returns whether this controller was created for a
  // Chrome App (platform app or legacy packaged app).
  virtual bool IsHostedApp() const;

  // Whether the custom tab bar should be visible.
  virtual bool ShouldShowCustomTabBar() const;

  // Whether the browser should include the tab strip.
  virtual bool has_tab_strip() const;

  // Whether the browser should show the menu button in the toolbar.
  virtual bool HasTitlebarMenuButton() const;

  // Whether to show app origin text in the titlebar toolbar.
  virtual bool HasTitlebarAppOriginText() const;

  // Whether to show content settings in the titlebar toolbar.
  virtual bool HasTitlebarContentSettings() const;

  // Returns which PageActionIconTypes should appear in the titlebar toolbar.
  virtual std::vector<PageActionIconType> GetTitleBarPageActions() const;

  // Whether to show the Back and Refresh buttons in the web app toolbar.
  virtual bool HasMinimalUiButtons() const = 0;

  // Returns the app icon for the window to use in the task list.
  virtual ui::ImageModel GetWindowAppIcon() const = 0;

  // Returns the icon to be displayed in the window title bar.
  virtual ui::ImageModel GetWindowIcon() const = 0;

  // Returns the color of the title bar.
  virtual std::optional<SkColor> GetThemeColor() const;

  // Returns the background color of the page.
  virtual std::optional<SkColor> GetBackgroundColor() const;

  // Returns the title to be displayed in the window title bar.
  virtual std::u16string GetTitle() const;

  // Gets the short name of the app.
  virtual std::u16string GetAppShortName() const = 0;

  // Returns the human-readable name for title in Media Controls.
  // If the returned value is an empty string, it means that there is no
  // human-readable name.
  std::string GetTitleForMediaControls() const;

  // Gets the origin of the app start url suitable for display (e.g
  // example.com.au).
  virtual std::u16string GetFormattedUrlOrigin() const = 0;

  // Gets the start_url for the app.
  virtual GURL GetAppStartUrl() const = 0;

  // Gets the new tab URL for tabbed apps.
  virtual GURL GetAppNewTabUrl() const;

  // Whether the app's tab strip should hide the new tab button, e.g. because
  // the app has a pinned home tab at the same URL as the new tab URL.
  virtual bool ShouldHideNewTabButton() const;

  // Returns whether the url is within the scope of the tab strip home tab.
  virtual bool IsUrlInHomeTabScope(const GURL& url) const;

  // Returns whether the app icon should be displayed on the tab instead of the
  // favicon.
  virtual bool ShouldShowAppIconOnTab(int index) const;

  // Determines whether the specified url is 'inside' the app |this| controls.
  virtual bool IsUrlInAppScope(const GURL& url) const = 0;

#if BUILDFLAG(IS_MAC)
  // Whether the toolbar should always be shown when in fullscreen mode.
  virtual bool AlwaysShowToolbarInFullscreen() const;
  virtual void ToggleAlwaysShowToolbarInFullscreen();
#endif

  // Safe downcast:
  virtual WebAppBrowserController* AsWebAppBrowserController();

  virtual bool CanUserUninstall() const;

  virtual void Uninstall(
      webapps::WebappUninstallSource webapp_uninstall_source);

  // Returns whether the app is installed (uninstallation may complete within
  // the lifetime of HostedAppBrowserController).
  virtual bool IsInstalled() const;

  // Returns an optional custom tab menu model factory.
  virtual std::unique_ptr<TabMenuModelFactory> GetTabMenuModelFactory() const;

  // Returns true when an app's effective display mode is
  // window-controls-overlay.
  virtual bool AppUsesWindowControlsOverlay() const;

  // Returns true when an app's effective display mode is borderless.
  virtual bool AppUsesBorderlessMode() const;

  // Returns true when an app's effective display mode is tabbed.
  virtual bool AppUsesTabbed() const;

  virtual bool IsIsolatedWebApp() const;

  // TODO(crbug.com/40222062): Remove this mock when `WebAppBrowserTest`s
  // support creating Isolated Web Apps.
  virtual void SetIsolatedWebAppTrueForTesting();

  // Returns true when the app's effective display mode is
  // window-controls-overlay and the user has toggled WCO on for the app.
  virtual bool IsWindowControlsOverlayEnabled() const;

  virtual void ToggleWindowControlsOverlayEnabled(
      base::OnceClosure on_complete);

  // Returns the default bounds for the app or empty for no defaults.
  virtual gfx::Rect GetDefaultBounds() const;

  // Whether the browser should show the reload button in the toolbar.
  virtual bool HasReloadButton() const;

  // Returns whether prevent close is enabled.
  bool IsPreventCloseEnabled() const;

#if !BUILDFLAG(IS_CHROMEOS)
  // Whether the browser should show the profile menu button in the toolbar.
  // Not appliccable to ChromeOS, because apps can be installed only for
  // one main profile there.
  virtual bool HasProfileMenuButton() const;
  virtual bool IsProfileMenuButtonVisible() const;
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns the SystemWebAppDelegate if any for this controller.
  virtual const ash::SystemWebAppDelegate* system_app() const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Updates the custom tab bar's visibility based on whether it should be
  // currently visible or not. If |animate| is set, the change will be
  // animated.
  void UpdateCustomTabBarVisibility(bool animate) const;

  const webapps::AppId& app_id() const { return app_id_; }

  Browser* browser() const { return browser_; }

  // Gets the url that the app browser controller was created with. Note: This
  // may be empty until the web contents begins navigating.
  const GURL& initial_url() const { return initial_url_; }

  // content::WebContentsObserver:
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void DidChangeThemeColor() override;
  void OnBackgroundColorChanged() override;
  void PrimaryPageChanged(content::Page& page) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // BrowserThemeProviderDelegate:
  CustomThemeSupplier* GetThemeSupplier() const override;
  bool ShouldUseCustomFrame() const override;

  // ui::ColorProviderKey::InitializerSupplier
  void AddColorMixers(ui::ColorProvider* provider,
                      const ui::ColorProviderKey& key) const override;

  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents);
  const std::optional<SkRegion>& draggable_region() const {
    return draggable_region_;
  }

  void SetOnUpdateDraggableRegionForTesting(base::OnceClosure done);

  // Called when this browser is going to receive a reparented web contents
  // from an installation or intent action. If the initial url is not set or
  // isn't within the app scope, set it to the app's start_url, allowing the 'x'
  // button to appear in the toolbar & the user can use it to navigate back to
  // that location.
  void MaybeSetInitialUrlOnReparentTab();

 protected:
  AppBrowserController(Browser* browser,
                       webapps::AppId app_id,
                       bool has_tab_strip);
  AppBrowserController(Browser* browser, webapps::AppId app_id);

  // Called once the app browser controller has determined its initial url.
  virtual void OnReceivedInitialURL();

  // Called by OnTabstripModelChanged().
  virtual void OnTabInserted(content::WebContents* contents);
  virtual void OnTabRemoved(content::WebContents* contents);

  // Gets the icon to use if the app icon is not available.
  ui::ImageModel GetFallbackAppIcon() const;

  void UpdateThemePack();

 private:
  // Sets the url that the app browser controller was created with.
  void SetInitialURL(const GURL& initial_url);

  // Indicates to the WebView whether it should support draggable regions via
  // the app-region CSS property.
  void UpdateSupportsDraggableRegions(bool supports_draggable_regions,
                                      content::RenderFrameHost* host);

  const raw_ptr<Browser> browser_;
  const webapps::AppId app_id_;
  const bool has_tab_strip_;
  GURL initial_url_;

  scoped_refptr<BrowserThemePack> theme_pack_;
  std::unique_ptr<ui::ThemeProvider> theme_provider_;
  std::optional<SkColor> last_theme_color_;
  std::optional<SkColor> last_background_color_;

  std::optional<SkRegion> draggable_region_ = std::nullopt;

  base::OnceClosure on_draggable_region_set_for_testing_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_APP_BROWSER_CONTROLLER_H_

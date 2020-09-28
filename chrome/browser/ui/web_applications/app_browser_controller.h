// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_APP_BROWSER_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkColor.h"

class Browser;
class BrowserThemePack;
class CustomThemeSupplier;
class TabMenuModelFactory;

namespace gfx {
class ImageSkia;
class Rect;
}  // namespace gfx

namespace web_app {

class WebAppBrowserController;
enum class SystemAppType;

// Returns true if |app_url| and |page_url| are the same origin. To avoid
// breaking Hosted Apps and Bookmark Apps that might redirect to sites in the
// same domain but with "www.", this returns true if |page_url| is secure and in
// the same origin as |app_url| with "www.".
bool IsSameHostAndPort(const GURL& app_url, const GURL& page_url);

// Class to encapsulate logic to control the browser UI for web apps.
class AppBrowserController : public TabStripModelObserver,
                             public content::WebContentsObserver,
                             public BrowserThemeProviderDelegate {
 public:
  ~AppBrowserController() override;

  static std::unique_ptr<AppBrowserController> MaybeCreateWebAppController(
      Browser* browser);

  // Returns whether |browser| uses the experimental hosted app experience.
  // Convenience wrapper for checking IsForExperimentalWebAppBrowser() on
  // |browser|'s HostedAppBrowserController if it exists.
  static bool IsForWebAppBrowser(const Browser* browser);
  static bool IsForWebAppBrowser(const Browser* browser, const AppId& app_id);

  // Renders |url|'s origin as Unicode.
  static base::string16 FormatUrlOrigin(
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

  // Returns whether this controller was created for an installed PWA.
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
  virtual gfx::ImageSkia GetWindowAppIcon() const = 0;

  // Returns the icon to be displayed in the window title bar.
  virtual gfx::ImageSkia GetWindowIcon() const = 0;

  // Returns the color of the title bar.
  virtual base::Optional<SkColor> GetThemeColor() const;

  // Returns the background color of the page.
  virtual base::Optional<SkColor> GetBackgroundColor() const;

  // Returns the title to be displayed in the window title bar.
  virtual base::string16 GetTitle() const;

  // Gets the short name of the app.
  virtual base::string16 GetAppShortName() const = 0;

  // Gets the origin of the app start url suitable for display (e.g
  // example.com.au).
  virtual base::string16 GetFormattedUrlOrigin() const = 0;

  // Gets the start_url for the app.
  virtual GURL GetAppStartUrl() const = 0;

  // Determines whether the specified url is 'inside' the app |this| controls.
  virtual bool IsUrlInAppScope(const GURL& url) const = 0;

  // Safe downcast:
  virtual WebAppBrowserController* AsWebAppBrowserController();

  virtual bool CanUninstall() const;

  virtual void Uninstall();

  // Returns whether the app is installed (uninstallation may complete within
  // the lifetime of HostedAppBrowserController).
  virtual bool IsInstalled() const;

  virtual std::unique_ptr<TabMenuModelFactory> GetTabMenuModelFactory() const;

  // Updates the custom tab bar's visibility based on whether it should be
  // currently visible or not. If |animate| is set, the change will be
  // animated.
  void UpdateCustomTabBarVisibility(bool animate) const;

  // Returns true if this controller is for a System Web App.
  bool is_for_system_web_app() const { return system_app_type_.has_value(); }

  // Returns the SystemAppType for this controller.
  const base::Optional<SystemAppType>& system_app_type() const {
    return system_app_type_;
  }

  // Returns true if AppId is non-null
  bool HasAppId() const { return app_id_.has_value(); }

  // Returns AppId if it is defined, otherwise DCHECK.
  // Should check HasAppId() before calling if unsure
  const AppId& GetAppId() const { return app_id_.value(); }

  Browser* browser() const { return browser_; }

  // Gets the url that the app browser controller was created with. Note: This
  // may be empty until the web contents begins navigating.
  const GURL& initial_url() const { return initial_url_; }

  // Returns the default bounds for the app or empty for no defaults.
  gfx::Rect GetDefaultBounds() const;

  // Returns whether the specified Tab Context Menu shortcut should be shown.
  bool ShouldShowTabContextMenuShortcut(int command_id) const;

  // content::WebContentsObserver:
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void DidChangeThemeColor() override;
  void OnBackgroundColorChanged() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // BrowserThemeProviderDelegate:
  CustomThemeSupplier* GetThemeSupplier() const override;

 protected:
  explicit AppBrowserController(Browser* browser,
                                base::Optional<web_app::AppId> app_id);

  // Called once the app browser controller has determined its initial url.
  virtual void OnReceivedInitialURL();

  // Called by OnTabstripModelChanged().
  virtual void OnTabInserted(content::WebContents* contents);
  virtual void OnTabRemoved(content::WebContents* contents);

  // Gets the icon to use if the app icon is not available.
  gfx::ImageSkia GetFallbackAppIcon() const;

 private:
  // Sets the url that the app browser controller was created with.
  void SetInitialURL(const GURL& initial_url);

  void UpdateThemePack();

  const base::Optional<AppId> app_id_;
  Browser* const browser_;
  GURL initial_url_;

  scoped_refptr<BrowserThemePack> theme_pack_;
  std::unique_ptr<ui::ThemeProvider> theme_provider_;
  base::Optional<SkColor> last_theme_color_;
  base::Optional<SkColor> last_background_color_;

  base::Optional<SystemAppType> system_app_type_;

  const bool has_tab_strip_;

  DISALLOW_COPY_AND_ASSIGN(AppBrowserController);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_APP_BROWSER_CONTROLLER_H_

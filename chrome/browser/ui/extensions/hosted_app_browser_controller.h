// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/engagement/site_engagement_observer.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "third_party/skia/include/core/SkColor.h"

class Browser;

namespace gfx {
class ImageSkia;
}

namespace extensions {

extern const char kPwaWindowEngagementTypeHistogram[];

class Extension;

// Class to encapsulate logic to control the browser UI for hosted apps.
class HostedAppBrowserController : public SiteEngagementObserver,
                                   public TabStripModelObserver,
                                   public ExtensionUninstallDialog::Delegate {
 public:
  // Returns whether |browser| uses the experimental hosted app experience.
  static bool IsForExperimentalHostedAppBrowser(const Browser* browser);

  // Functions to set preferences that are unique to app windows.
  static void SetAppPrefsForWebContents(HostedAppBrowserController* controller,
                                        content::WebContents* web_contents);

  // Renders |url|'s origin as Unicode.
  static base::string16 FormatUrlOrigin(const GURL& url);

  explicit HostedAppBrowserController(Browser* browser);
  ~HostedAppBrowserController() override;

  // Returns true if the associated Hosted App is for a PWA.
  bool created_for_installed_pwa() const { return created_for_installed_pwa_; }

  // Whether the browser being controlled should be currently showing the
  // location bar.
  bool ShouldShowLocationBar() const;

  // Updates the location bar visibility based on whether it should be
  // currently visible or not. If |animate| is set, the change will be
  // animated.
  void UpdateLocationBarVisibility(bool animate) const;

  // Returns the app icon for the window to use in the task list.
  gfx::ImageSkia GetWindowAppIcon() const;

  // Returns the icon to be displayed in the window title bar.
  gfx::ImageSkia GetWindowIcon() const;

  // Returns the color of the title bar.
  base::Optional<SkColor> GetThemeColor() const;

  // Returns the title to be displayed in the window title bar.
  base::string16 GetTitle() const;

  // Gets the short name of the app.
  std::string GetAppShortName() const;

  // Gets the origin of the app start url suitable for display (e.g
  // example.com.au).
  base::string16 GetFormattedUrlOrigin() const;

  // Gets the extension for this controller.
  const Extension* GetExtensionForTesting() const;

  bool CanUninstall() const;

  void Uninstall(UninstallReason reason, UninstallSource source);

  // SiteEngagementObserver overrides.
  void OnEngagementEvent(content::WebContents* web_contents,
                         const GURL& url,
                         double score,
                         SiteEngagementService::EngagementType type) override;

  // TabStripModelObserver overrides.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  // Called by OnTabstripModelChanged().
  void OnTabInserted(content::WebContents* contents);
  void OnTabRemoved(content::WebContents* contents);

  const Extension* GetExtension() const;

  Browser* const browser_;
  const std::string extension_id_;
  const bool created_for_installed_pwa_;
  std::unique_ptr<ExtensionUninstallDialog> uninstall_dialog_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppBrowserController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_

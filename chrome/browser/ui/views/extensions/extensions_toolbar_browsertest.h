// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BROWSERTEST_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BROWSERTEST_H_

#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace extensions {
class ExtensionService;
}  // namespace extensions

class ExtensionsMenuCoordinator;
class ExtensionsRequestAccessButton;
class ExtensionsToolbarButton;
class ToolbarActionView;

// Base class for browser tests that test the extensions toolbar and menu views.
class ExtensionsToolbarBrowserTest : public InProcessBrowserTest {
 public:
  ExtensionsToolbarBrowserTest();
  ExtensionsToolbarBrowserTest(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features);
  ~ExtensionsToolbarBrowserTest() override;
  ExtensionsToolbarBrowserTest(const ExtensionsToolbarBrowserTest&) = delete;
  const ExtensionsToolbarBrowserTest& operator=(
      const ExtensionsToolbarBrowserTest&) = delete;

  Profile* profile() { return browser()->GetProfile(); }

  extensions::ExtensionService* extension_service() {
    return extension_service_;
  }

  extensions::ExtensionRegistrar* extension_registrar() {
    return extensions::ExtensionRegistrar::Get(profile());
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  ExtensionsToolbarDesktop* extensions_container() {
    return browser_view()->toolbar()->extensions_container();
  }

  ExtensionsToolbarButton* extensions_button() {
    return extensions_container()->GetExtensionsButton();
  }

  ExtensionsRequestAccessButton* request_access_button() {
    return extensions_container()->GetRequestAccessButton();
  }

  ExtensionsMenuCoordinator* menu_coordinator() {
    return extensions_container()->GetExtensionsMenuCoordinatorForTesting();
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Adds the specified `extension`.
  scoped_refptr<const extensions::Extension> InstallExtension(
      const std::string& name);

  // Adds the specified `extension` with the given `host_permissions`.
  scoped_refptr<const extensions::Extension>
  InstallExtensionWithHostPermissions(
      const std::string& name,
      const std::vector<std::string>& host_permissions);

  // Adds the specified `extension` with the given `permissions`.
  scoped_refptr<const extensions::Extension> InstallExtensionWithPermissions(
      const std::string& name,
      const std::vector<std::string>& permissions);

  scoped_refptr<const extensions::Extension> InstallEnterpriseExtension(
      const std::string& name,
      const std::vector<std::string>& host_permissions);

  // Adds the specified `extension` with the given `host_permissions`,
  // `permissions` and `location`.
  scoped_refptr<const extensions::Extension> InstallExtension(
      const std::string& name,
      const std::vector<std::string>& permissions,
      const std::vector<std::string>& host_permissions,
      extensions::mojom::ManifestLocation location =
          extensions::mojom::ManifestLocation::kUnpacked);

  // Reloads the extension of the given `extension_id`.
  void ReloadExtension(const extensions::ExtensionId& extension_id);

  // Uninstalls the extensions of the given `extension_id`.
  void UninstallExtension(const extensions::ExtensionId& extension_id);

  // Enables the extension of the given `extension_id`.
  void EnableExtension(const extensions::ExtensionId& extension_id);

  // Disables the extension of the given `extension_id`.
  void DisableExtension(const extensions::ExtensionId& extension_id);

  // Withhold all host permissions of the given `extension`.
  void WithholdHostPermissions(const extensions::Extension* extension);

  // Triggers the press and release event of the given `button`.
  void ClickButton(views::Button* button) const;

  // Updates the user's site access for `extension` on `web_contents` to
  // `site_access`.
  void UpdateUserSiteAccess(
      const extensions::Extension& extension,
      content::WebContents* web_contents,
      extensions::PermissionsManager::UserSiteAccess site_access);

  // Updates the user's site setting to `site_setting` for `url`.
  void UpdateUserSiteSetting(
      extensions::PermissionsManager::UserSiteSetting site_setting,
      const GURL& url);

  // Adds a site access request with an optional `filter` for `extension` in
  // `web_contents`.
  void AddHostAccessRequest(
      const extensions::Extension& extension,
      content::WebContents* web_contents,
      const std::optional<URLPattern>& filter = std::nullopt);

  // Removes the site access request for `extension` in `web_contents`, if
  // existent.
  void RemoveHostAccessRequest(const extensions::Extension& extension,
                               content::WebContents* web_contents);

  // Returns the user's site setting for `url`.
  extensions::PermissionsManager::UserSiteSetting GetUserSiteSetting(
      const GURL& url);

  // Returns the user's `extension` site access for `url`.
  extensions::PermissionsManager::UserSiteAccess GetUserSiteAccess(
      const extensions::Extension& extension,
      const GURL& url) const;

  // Returns the `extension` site interaction on `web_contents`.
  extensions::SitePermissionsHelper::SiteInteraction GetSiteInteraction(
      const extensions::Extension& extension,
      content::WebContents* web_contents) const;

  // Returns a list of the views of the currently pinned extensions, in order
  // from left to right.
  std::vector<ToolbarActionView*> GetPinnedExtensionViews();

  // Returns a list of the names of the currently pinned extensions, in order
  // from left to right.
  std::vector<std::string> GetPinnedExtensionNames();

  // Waits for the extensions container to animate (on pin, unpin, pop-out,
  // etc.)
  void WaitForAnimation();

  // Navigates the active tab to `url` and waits for animation.
  void NavigateAndCommit(const GURL& url);

  // Since this is a test, the ExtensionsToolbarDesktop sometimes needs a
  // nudge to re-layout the views.
  void LayoutContainerIfNecessary();

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<extensions::ExtensionService> extension_service_ = nullptr;
  raw_ptr<extensions::PermissionsManager> permissions_manager_ = nullptr;
  std::unique_ptr<extensions::SitePermissionsHelper> permissions_helper_;
  std::optional<base::AutoReset<base::TimeDelta>> cooldown_reset_;
  std::optional<base::AutoReset<std::optional<bool>>>
      accept_reload_dialog_reset_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BROWSERTEST_H_

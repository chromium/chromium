// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_UNITTEST_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_UNITTEST_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"

namespace extensions {
class ExtensionService;
}  // namespace extensions

// Base class for unit tests that use the toolbar area. This is used for unit
// tests that are generally related to the ExtensionsToolbarContainer in the
// ToolbarView area (e.g ExtensionsToolbarControls).
// When possible, prefer creating a unit test with browser view instead of a
// interactive ui or browser test since they are faster and less flaky.
class ExtensionsToolbarUnitTest : public TestWithBrowserView {
 public:
  ExtensionsToolbarUnitTest();
  explicit ExtensionsToolbarUnitTest(
      base::test::TaskEnvironment::TimeSource time_source);
  ~ExtensionsToolbarUnitTest() override;
  ExtensionsToolbarUnitTest(const ExtensionsToolbarUnitTest&) = delete;
  const ExtensionsToolbarUnitTest& operator=(const ExtensionsToolbarUnitTest&) =
      delete;

  extensions::ExtensionService* extension_service() {
    return extension_service_;
  }

  ExtensionsToolbarContainer* extensions_container() {
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
  void AddSiteAccessRequest(
      const extensions::Extension& extension,
      content::WebContents* web_contents,
      const std::optional<URLPattern>& filter = std::nullopt);

  // Removes the site access request for `extension` in `web_contents`, if
  // existent.
  void RemoveSiteAccessRequest(const extensions::Extension& extension,
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

  // Since this is a unittest, the ExtensionsToolbarContainer sometimes needs a
  // nudge to re-layout the views.
  void LayoutContainerIfNecessary();

  // Adds a new tab to the tab strip, and returns the WebContentsTester
  // associated with it.
  content::WebContentsTester* AddWebContentsAndGetTester();

  // TestWithBrowserView:
  void SetUp() override;
  void TearDown() override;

 private:
  raw_ptr<extensions::ExtensionService, DanglingUntriaged> extension_service_ =
      nullptr;
  raw_ptr<extensions::PermissionsManager, DanglingUntriaged>
      permissions_manager_ = nullptr;
  std::unique_ptr<extensions::SitePermissionsHelper> permissions_helper_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_UNITTEST_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_platform_delegate.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace {

// A mock extensions menu platform delegate.
class TestPlatformDelegate : public ExtensionsMenuViewPlatformDelegate {
 public:
  TestPlatformDelegate() = default;
  ~TestPlatformDelegate() override = default;

  void AttachToModel(ExtensionsMenuViewModel* model) override {}
  void DetachFromModel() override {}
};

}  // namespace

class ExtensionsMenuViewModelBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionsMenuViewModelBrowserTest() = default;
  ~ExtensionsMenuViewModelBrowserTest() override = default;

  // Adds an extension with the given `host_permission`.
  scoped_refptr<const extensions::Extension> AddExtension(
      const std::string& name,
      const std::string& host_permission);

  // ExtensionBrowserTest:
  void SetUpOnMainThread() override;
};

void ExtensionsMenuViewModelBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
}

scoped_refptr<const extensions::Extension>
ExtensionsMenuViewModelBrowserTest::AddExtension(
    const std::string& name,
    const std::string& host_permission) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetID(crx_file::id_util::GenerateId(name))
          .AddHostPermission(host_permission)
          .Build();
  extension_registrar()->AddExtension(extension.get());
  return extension;
}

// Tests that the extensions menu view model correctly updates the site access
// for an extension.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest, UpdateSiteAccess) {
  // Add extension that requests host permissions.
  scoped_refptr<const extensions::Extension> extension =
      AddExtension("Extension", "<all_urls>");

  // Navigate to a site the extension has site access to.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
  content::WebContents* web_contents = GetActiveWebContents();

  auto model = std::make_unique<ExtensionsMenuViewModel>(
      browser(), std::make_unique<TestPlatformDelegate>());

  // Verify default initial site access is "on all sites".
  extensions::PermissionsManager* permissions_manager =
      extensions::PermissionsManager::Get(profile());
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            extensions::PermissionsManager::UserSiteAccess::kOnAllSites);

  // Update site access to "on site".
  model->UpdateSiteAccess(
      extension->id(), extensions::PermissionsManager::UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            extensions::PermissionsManager::UserSiteAccess::kOnSite);

  // Update site access to "on click".
  model->UpdateSiteAccess(
      extension->id(),
      extensions::PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_manager->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            extensions::PermissionsManager::UserSiteAccess::kOnClick);
}

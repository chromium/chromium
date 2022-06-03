// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_controls.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view_utils.h"

namespace {

std::unique_ptr<base::ListValue> ToListValue(
    const std::vector<std::string>& permissions) {
  extensions::ListBuilder builder;
  for (const std::string& permission : permissions)
    builder.Append(permission);
  return builder.Build();
}

}  // namespace

class ExtensionsToolbarControlsTest : public TestWithBrowserView {
 public:
  ExtensionsToolbarControlsTest();
  ~ExtensionsToolbarControlsTest() override = default;
  ExtensionsToolbarControlsTest(const ExtensionsToolbarControlsTest&) = delete;
  const ExtensionsToolbarControlsTest& operator=(
      const ExtensionsToolbarControlsTest&) = delete;

  // TestWithBrowserView:
  void SetUp() override;

  extensions::ExtensionService* extension_service() {
    return extension_service_;
  }

  ExtensionsToolbarContainer* extensions_container() {
    return browser_view()->toolbar()->extensions_container();
  }

  // Adds an extension with no host permissions.
  const extensions::Extension* InstallExtension(const std::string& name);

  // Adds an extension with the given host permission.
  const extensions::Extension* InstallExtensionWithHostPermissions(
      const std::string& name,
      const std::vector<std::string>& host_permissions);

  // Uninstalls the given extension.
  void UninstallExtension(const extensions::Extension* extension);

  // Adds a new tab to the tab strip, and returns the WebContentsTester
  // associated with it.
  content::WebContentsTester* AddWebContentsAndGetTester();

  // Returns whether the site access button is visible or not.
  bool IsSiteAccessButtonVisible();

  // Since this is a unittest, the ExtensionsToolbarContainer sometimes needs a
  // nudge to re-layout the views.
  void LayoutContainerIfNecessary();

 private:
  extensions::ExtensionService* extension_service_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

ExtensionsToolbarControlsTest::ExtensionsToolbarControlsTest() {
  scoped_feature_list_.InitAndEnableFeature(
      features::kExtensionsMenuAccessControl);
}

void ExtensionsToolbarControlsTest::SetUp() {
  TestWithBrowserView::SetUp();

  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

  extension_service_ =
      extensions::ExtensionSystem::Get(profile())->extension_service();

  // Shorten delay on animations so tests run faster.
  views::test::ReduceAnimationDuration(extensions_container());
}

const extensions::Extension* ExtensionsToolbarControlsTest::InstallExtension(
    const std::string& name) {
  return InstallExtensionWithHostPermissions(name, {});
}

const extensions::Extension*
ExtensionsToolbarControlsTest::InstallExtensionWithHostPermissions(
    const std::string& name,
    const std::vector<std::string>& host_permissions) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetManifestKey("manifest_version", 3)
          .SetManifestKey("host_permissions", ToListValue(host_permissions))
          .SetID(crx_file::id_util::GenerateId(name))
          .Build();
  extension_service()->AddExtension(extension.get());

  // Force the container to re-layout, since a new extension was added.
  LayoutContainerIfNecessary();

  return extension.get();
}

void ExtensionsToolbarControlsTest::UninstallExtension(
    const extensions::Extension* extension) {
  extension_service()->UninstallExtension(
      extension->id(),
      extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING, nullptr);

  // Force the container to re-layout, since a new extension was added.
  LayoutContainerIfNecessary();
}

content::WebContentsTester*
ExtensionsToolbarControlsTest::AddWebContentsAndGetTester() {
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* raw_contents = contents.get();
  browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), raw_contents);
  return content::WebContentsTester::For(raw_contents);
}

bool ExtensionsToolbarControlsTest::IsSiteAccessButtonVisible() {
  for (auto* view : extensions_container()->children()) {
    if (views::IsViewClass<ExtensionsToolbarControls>(view))
      return static_cast<ExtensionsToolbarControls*>(view)
          ->site_access_button_for_testing()
          ->GetVisible();
  }
  return false;
}

void ExtensionsToolbarControlsTest::LayoutContainerIfNecessary() {
  extensions_container()->GetWidget()->LayoutRootViewIfNecessary();
}

TEST_F(ExtensionsToolbarControlsTest,
       SiteAccessButtonVisibility_NavigationBetweenPages) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // Add an extension that only requests access to a specific url.
  InstallExtensionWithHostPermissions("specific_url", {url_a.spec()});
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Navigate to an url the extension should have access to.
  web_contents_tester->NavigateAndCommit(url_a);
  EXPECT_TRUE(IsSiteAccessButtonVisible());

  // Navigate to an url the extension should not have access to.
  web_contents_tester->NavigateAndCommit(url_b);
  EXPECT_FALSE(IsSiteAccessButtonVisible());
}

TEST_F(ExtensionsToolbarControlsTest,
       SiteAccessButtonVisibility_ContextMenuChangesHostPermissions) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // Add an extension with all urls host permissions. Since we haven't navigated
  // to an url yet, the extension should not have access.
  auto* extension =
      InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Navigate to an url the extension should have access to as part of
  // <all_urls>.
  web_contents_tester->NavigateAndCommit(url_a);
  EXPECT_TRUE(IsSiteAccessButtonVisible());

  // Change the extension to run only on the current site using the context
  // menu. The extension should still have access to the current site.
  extensions::ExtensionContextMenuModel context_menu(
      extension, browser(), extensions::ExtensionContextMenuModel::PINNED,
      nullptr, true);
  context_menu.ExecuteCommand(
      extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE, 0);
  EXPECT_TRUE(IsSiteAccessButtonVisible());

  // Navigate to a different url. The extension should not have access.
  web_contents_tester->NavigateAndCommit(url_b);
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Go back to the original url. The extension should have access.
  web_contents_tester->NavigateAndCommit(url_a);
  EXPECT_TRUE(IsSiteAccessButtonVisible());
}

TEST_F(ExtensionsToolbarControlsTest,
       SiteAccessButtonVisibility_MultipleExtensions) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // There are no extensions installed yet, so no extension has access to the
  // current site.
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Add an extension that doesn't request host permissions. Extension should
  // not have access to the current site.
  InstallExtension("no_permissions");
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Add an extension that only requests access to url_a. Extension should not
  // have access to the current site.
  InstallExtensionWithHostPermissions("specific_url", {url_a.spec()});
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Add an extension with all urls host permissions. Extension should not have
  // access because there isn't a real url yet.
  auto* extension_all_urls =
      InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Navigate to the url that "specific_url" extension has access to. Both
  // "all_urls" and "specific_urls" should have accessn to the current site.
  web_contents_tester->NavigateAndCommit(url_a);
  EXPECT_TRUE(IsSiteAccessButtonVisible());

  // Navigate to a different url. Only "all_urls" should have access.
  web_contents_tester->NavigateAndCommit(url_b);
  EXPECT_TRUE(IsSiteAccessButtonVisible());

  // Remove the only extension that has access to the current site. No extension
  // should have access to the current site.
  UninstallExtension(extension_all_urls);
  EXPECT_FALSE(IsSiteAccessButtonVisible());
}

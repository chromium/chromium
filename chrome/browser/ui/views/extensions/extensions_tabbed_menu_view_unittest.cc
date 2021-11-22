// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"

#include "base/feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/button_test_api.h"

namespace {

std::unique_ptr<base::ListValue> ToListValue(
    const std::vector<std::string>& permissions) {
  extensions::ListBuilder builder;
  for (const std::string& permission : permissions)
    builder.Append(permission);
  return builder.Build();
}

}  // namespace

class ExtensionsTabbedMenuViewUnitTest : public TestWithBrowserView {
 public:
  ExtensionsTabbedMenuViewUnitTest();
  ~ExtensionsTabbedMenuViewUnitTest() override = default;
  ExtensionsTabbedMenuViewUnitTest(const ExtensionsTabbedMenuViewUnitTest&) =
      delete;
  ExtensionsTabbedMenuViewUnitTest& operator=(
      const ExtensionsTabbedMenuViewUnitTest&) = delete;

  // TestWithBrowserView:
  void SetUp() override;

  extensions::ExtensionService* extension_service() {
    return extension_service_;
  }
  ExtensionsToolbarButton* extensions_button() {
    return browser_view()
        ->toolbar()
        ->extensions_container()
        ->GetExtensionsToolbarControls()
        ->extensions_button();
  }
  ExtensionsToolbarButton* site_access_button() {
    return browser_view()
        ->toolbar()
        ->extensions_container()
        ->GetExtensionsToolbarControls()
        ->site_access_button_for_testing();
  }
  ExtensionsTabbedMenuView* extensions_tabbed_menu() {
    return ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting();
  }

  // Adds a new tab to the tab strip, and returns the WebContentsTester
  // associated with it.
  content::WebContentsTester* AddWebContentsAndGetTester();

  // Adds an extension with no host permissions.
  const extensions::Extension* InstallExtension(const std::string& name);

  // Adds an extension with the given host permission.
  const extensions::Extension* InstallExtensionWithHostPermissions(
      const std::string& name,
      const std::vector<std::string>& host_permissions);

  // Triggers the pressed event of the given `button`.
  void ClickToolbarButton(ExtensionsToolbarButton* button);

  // Since this is a unittest, the ExtensionsToolbarContainer sometimes needs a
  // nudge to re-layout the views.
  void LayoutContainerIfNecessary();

 private:
  extensions::ExtensionService* extension_service_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

ExtensionsTabbedMenuViewUnitTest::ExtensionsTabbedMenuViewUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      features::kExtensionsMenuAccessControl);
}

void ExtensionsTabbedMenuViewUnitTest::SetUp() {
  TestWithBrowserView::SetUp();

  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

  extension_service_ =
      extensions::ExtensionSystem::Get(profile())->extension_service();

  // Shorten delay on animations so tests run faster.
  views::test::ReduceAnimationDuration(
      browser_view()->toolbar()->extensions_container());
}

content::WebContentsTester*
ExtensionsTabbedMenuViewUnitTest::AddWebContentsAndGetTester() {
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* raw_contents = contents.get();
  browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), raw_contents);
  return content::WebContentsTester::For(raw_contents);
}

const extensions::Extension* ExtensionsTabbedMenuViewUnitTest::InstallExtension(
    const std::string& name) {
  return InstallExtensionWithHostPermissions(name, {});
}

const extensions::Extension*
ExtensionsTabbedMenuViewUnitTest::InstallExtensionWithHostPermissions(
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

void ExtensionsTabbedMenuViewUnitTest::ClickToolbarButton(
    ExtensionsToolbarButton* button) {
  button->OnMousePressed(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                        gfx::Point(), ui::EventTimeForNow(),
                                        ui::EF_LEFT_MOUSE_BUTTON, 0));
  LayoutContainerIfNecessary();
}

void ExtensionsTabbedMenuViewUnitTest::LayoutContainerIfNecessary() {
  browser_view()
      ->toolbar()
      ->extensions_container()
      ->GetWidget()
      ->LayoutRootViewIfNecessary();
}

TEST_F(ExtensionsTabbedMenuViewUnitTest, ButtonOpensAndClosesCorrespondingTab) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();

  // Load an extension with all urls permissions so the site access button is
  // visible.
  InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});

  // Navigate to an url where the extension should have access to.
  const GURL url("http://www.a.com");
  web_contents_tester->NavigateAndCommit(url);
  EXPECT_TRUE(site_access_button()->GetVisible());
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the extensions button when the menu is closed. Extensions menu
  // should open in the installed extensions tab.
  ClickToolbarButton(extensions_button());
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());
  EXPECT_EQ(extensions_tabbed_menu()->GetSelectedTabIndex(), 1u);

  // Click on the extensions button when the menu is open. Extensions menu
  // should be closed.
  ClickToolbarButton(extensions_button());
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the site access button when the menu is closed. Extensions menu
  // should open in the site access tab.
  ClickToolbarButton(site_access_button());
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());
  EXPECT_EQ(extensions_tabbed_menu()->GetSelectedTabIndex(), 0u);

  // Click on the site access button when the menu is open. Extensions menu
  // should close.
  ClickToolbarButton(site_access_button());
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());
}

TEST_F(ExtensionsTabbedMenuViewUnitTest, TogglingButtonsClosesMenu) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();

  // Load an extension with all urls permissions so the site access button is
  // visible.
  InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});

  // Navigate to an url where the extension should have access to.
  const GURL url("http://www.a.com");
  web_contents_tester->NavigateAndCommit(url);
  EXPECT_TRUE(site_access_button()->GetVisible());
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the extensions button when the menu is closed. Extensions menu
  // should open.
  ClickToolbarButton(extensions_button());
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the site access button when the menu is open. Extensions menu
  // should close since the button click is treated as a click outside the menu,
  // and therefore closing the menu, instead of triggering the button's click
  // action.
  // TODO(crbug.com/1263311): Toggle to the corresponding tab when clicking on
  // the other control when the menu is open.
  ClickToolbarButton(site_access_button());
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the site access button when the menu is closed. Extensions menu
  // should open.
  ClickToolbarButton(site_access_button());
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the extensions button when the menu is open. Extensions menu
  // should close, as explained previously.
  ClickToolbarButton(extensions_button());
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());
}

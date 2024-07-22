// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_interactive_uitest.h"

#include <vector>

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view_utils.h"

ExtensionsToolbarUITest::ExtensionsToolbarUITest() = default;

ExtensionsToolbarUITest::~ExtensionsToolbarUITest() = default;

Profile* ExtensionsToolbarUITest::profile() {
  return browser()->profile();
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarUITest::LoadTestExtension(const std::string& path,
                                           bool allow_incognito) {
  extensions::ChromeTestExtensionLoader loader(profile());
  loader.set_allow_incognito_access(allow_incognito);
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(test_data_dir.AppendASCII(path));
  AppendExtension(extension);

  // Loading an extension can result in the container changing visibility.
  // Allow it to finish laying out appropriately.
  auto* container = GetExtensionsToolbarContainer();
  container->GetWidget()->LayoutRootViewIfNecessary();
  return extension;
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarUITest::ForceInstallExtension(const std::string& name) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetManifestVersion(3)
          .SetLocation(extensions::mojom::ManifestLocation::kExternalPolicy)
          .SetID(crx_file::id_util::GenerateId(name))
          .Build();
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(extension.get());
  return extension;
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarUITest::InstallExtension(const std::string& name) {
  return InstallExtensionWithHostPermissions(name, std::string());
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarUITest::InstallExtensionWithHostPermissions(
    const std::string& name,
    const std::string& host_permission,
    const std::string& content_script_run_location) {
  std::string host_permissions_entry;
  if (!host_permission.empty()) {
    host_permissions_entry = base::StringPrintf(
        R"(
        "host_permissions": ["%s"],
      )",
        host_permission.c_str());
  }

  extensions::TestExtensionDir extension_dir;
  std::string content_script_entry;
  if (!content_script_run_location.empty()) {
    content_script_entry = base::StringPrintf(
        R"(
          "content_scripts": [{
            "matches": ["%s"],
            "js": ["script.js"],
            "run_at": "%s"
          }], )",
        host_permission.c_str(), content_script_run_location.c_str());

    extension_dir.WriteFile(FILE_PATH_LITERAL("script.js"),
                            base::StringPrintf("chrome.test.sendMessage('%s');",
                                               "injection succeeded"));
  }

  extension_dir.WriteManifest(base::StringPrintf(
      R"({
            "name": "%s",
            "manifest_version": 3,
            %s
            %s
            "version": "0.1"
          })",
      name.c_str(), content_script_entry.c_str(),
      host_permissions_entry.c_str()));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ChromeTestExtensionLoader(profile()).LoadExtension(
          extension_dir.UnpackedPath());
  AppendExtension(extension);
  return extension;
}

void ExtensionsToolbarUITest::AppendExtension(
    scoped_refptr<const extensions::Extension> extension) {
  extensions_.push_back(std::move(extension));
}

void ExtensionsToolbarUITest::DisableExtension(
    const extensions::ExtensionId& extension_id) {
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->DisableExtension(extension_id,
                         extensions::disable_reason::DISABLE_USER_ACTION);
}

void ExtensionsToolbarUITest::SetUpIncognitoBrowser() {
  incognito_browser_ = CreateIncognitoBrowser();
}

void ExtensionsToolbarUITest::SetUpOnMainThread() {
  DialogBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  views::test::ReduceAnimationDuration(GetExtensionsToolbarContainer());
}

ExtensionsToolbarContainer*
ExtensionsToolbarUITest::GetExtensionsToolbarContainer() const {
  return GetExtensionsToolbarContainerForBrowser(browser());
}

ExtensionsToolbarContainer*
ExtensionsToolbarUITest::GetExtensionsToolbarContainerForBrowser(
    Browser* browser) const {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar()
      ->extensions_container();
}

std::vector<ToolbarActionView*> ExtensionsToolbarUITest::GetToolbarActionViews()
    const {
  return GetToolbarActionViewsForBrowser(browser());
}

std::vector<ToolbarActionView*>
ExtensionsToolbarUITest::GetToolbarActionViewsForBrowser(
    Browser* browser) const {
  std::vector<ToolbarActionView*> views;
  for (views::View* view :
       GetExtensionsToolbarContainerForBrowser(browser)->children()) {
    if (views::IsViewClass<ToolbarActionView>(view))
      views.push_back(static_cast<ToolbarActionView*>(view));
  }
  return views;
}

std::vector<ToolbarActionView*>
ExtensionsToolbarUITest::GetVisibleToolbarActionViews() const {
  auto views = GetToolbarActionViews();
  std::erase_if(views, [](views::View* view) { return !view->GetVisible(); });
  return views;
}

ExtensionsToolbarButton* ExtensionsToolbarUITest::extensions_button() {
  return GetExtensionsToolbarContainer()->GetExtensionsButton();
}

ExtensionsMenuCoordinator* ExtensionsToolbarUITest::menu_coordinator() {
  return GetExtensionsToolbarContainer()
      ->GetExtensionsMenuCoordinatorForTesting();
}

bool ExtensionsToolbarUITest::DidInjectScript(
    content::WebContents* web_contents) {
  return extensions::browsertest_util::DidChangeTitle(
      *web_contents, /*original_title=*/u"OK",
      /*changed_title=*/u"success");
}

void ExtensionsToolbarUITest::NavigateTo(const GURL& url) {
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
}

void ExtensionsToolbarUITest::AddSiteAccessRequest(
    const extensions::Extension& extension,
    content::WebContents* web_contents) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  extensions::PermissionsManager::Get(profile())->AddSiteAccessRequest(
      web_contents, tab_id, extension);
}

void ExtensionsToolbarUITest::ClickButton(views::Button* button) const {
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), base::TimeTicks(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(),
                               gfx::Point(), base::TimeTicks(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
}

void ExtensionsToolbarUITest::WaitForAnimation() {
  views::test::WaitForAnimatingLayoutManager(GetExtensionsToolbarContainer());
}

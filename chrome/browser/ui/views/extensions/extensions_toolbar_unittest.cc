// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view_utils.h"

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

ExtensionsToolbarUnitTest::ExtensionsToolbarUnitTest() = default;

ExtensionsToolbarUnitTest::ExtensionsToolbarUnitTest(
    base::test::TaskEnvironment::TimeSource time_source)
    : TestWithBrowserView(time_source) {}

ExtensionsToolbarUnitTest::~ExtensionsToolbarUnitTest() = default;

void ExtensionsToolbarUnitTest::SetUp() {
  TestWithBrowserView::SetUp();

  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

  extension_service_ =
      extensions::ExtensionSystem::Get(profile())->extension_service();

  permissions_manager_ = PermissionsManager::Get(profile());
  permissions_helper_ = std::make_unique<SitePermissionsHelper>(profile());

  // Shorten delay on animations so tests run faster.
  views::test::ReduceAnimationDuration(extensions_container());
}

void ExtensionsToolbarUnitTest::TearDown() {
  // Avoid dangling pointer to profile.
  permissions_helper_.reset(nullptr);

  TestWithBrowserView::TearDown();
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarUnitTest::InstallExtension(const std::string& name) {
  return InstallExtension(name, {}, {});
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarUnitTest::InstallExtensionWithHostPermissions(
    const std::string& name,
    const std::vector<std::string>& host_permissions) {
  return InstallExtension(name, {}, host_permissions);
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarUnitTest::InstallExtensionWithPermissions(
    const std::string& name,
    const std::vector<std::string>& permissions) {
  return InstallExtension(name, permissions, {});
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarUnitTest::InstallEnterpriseExtension(
    const std::string& name,
    const std::vector<std::string>& host_permissions) {
  return InstallExtension(name, {}, host_permissions,
                          extensions::mojom::ManifestLocation::kExternalPolicy);
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarUnitTest::InstallExtension(
    const std::string& name,
    const std::vector<std::string>& permissions,
    const std::vector<std::string>& host_permissions,
    extensions::mojom::ManifestLocation location) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetManifestVersion(3)
          .SetLocation(location)
          .AddAPIPermissions(permissions)
          .AddHostPermissions(host_permissions)
          .SetID(crx_file::id_util::GenerateId(name))
          .Build();
  extension_service()->AddExtension(extension.get());

  // Force the container to re-layout, since a new extension was added.
  LayoutContainerIfNecessary();

  return extension;
}

void ExtensionsToolbarUnitTest::ReloadExtension(
    const extensions::ExtensionId& extension_id) {
  extension_service()->ReloadExtension(extension_id);
}

void ExtensionsToolbarUnitTest::UninstallExtension(
    const extensions::ExtensionId& extension_id) {
  // In some cases, exiting the test too early could cause it to fail,
  // because a worker thread is holding a lock to files it's trying to delete.
  // This prevents the test's temp dir from cleaning up properly.
  //
  // This is also a known bug for Ephemeral Profiles. NukeProfileFromDisk() can
  // race with a bunch of things, and extension uninstall is just one of them.
  // See crbug.com/1191455.
  base::RunLoop run_loop;
  extension_service()->UninstallExtension(
      extension_id, extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING,
      nullptr, run_loop.QuitClosure());
  run_loop.Run();
}

void ExtensionsToolbarUnitTest::EnableExtension(
    const extensions::ExtensionId& extension_id) {
  extension_service()->EnableExtension(extension_id);
}

void ExtensionsToolbarUnitTest::DisableExtension(
    const extensions::ExtensionId& extension_id) {
  extension_service()->DisableExtension(
      extension_id, extensions::disable_reason::DISABLE_USER_ACTION);
}

void ExtensionsToolbarUnitTest::WithholdHostPermissions(
    const extensions::Extension* extension) {
  extensions::PermissionsManagerWaiter waiter(permissions_manager_);
  extensions::ScriptingPermissionsModifier(profile(), extension)
      .RemoveAllGrantedHostPermissions();
  waiter.WaitForExtensionPermissionsUpdate();
}

void ExtensionsToolbarUnitTest::ClickButton(views::Button* button) const {
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
}

void ExtensionsToolbarUnitTest::UpdateUserSiteAccess(
    const extensions::Extension& extension,
    content::WebContents* web_contents,
    PermissionsManager::UserSiteAccess site_access) {
  extensions::PermissionsManagerWaiter waiter(
      PermissionsManager::Get(browser()->profile()));
  permissions_helper_->UpdateSiteAccess(extension, web_contents, site_access);
  waiter.WaitForExtensionPermissionsUpdate();
}

void ExtensionsToolbarUnitTest::UpdateUserSiteSetting(
    extensions::PermissionsManager::UserSiteSetting site_setting,
    const GURL& url) {
  extensions::PermissionsManagerWaiter waiter(permissions_manager_);
  permissions_manager_->UpdateUserSiteSetting(url::Origin::Create(url),
                                              site_setting);
  waiter.WaitForUserPermissionsSettingsChange();
}

void ExtensionsToolbarUnitTest::AddSiteAccessRequest(
    const extensions::Extension& extension,
    content::WebContents* web_contents,
    const std::optional<URLPattern>& filter) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  permissions_manager_->AddSiteAccessRequest(web_contents, tab_id, extension,
                                             filter);
}

void ExtensionsToolbarUnitTest::RemoveSiteAccessRequest(
    const extensions::Extension& extension,
    content::WebContents* web_contents) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  permissions_manager_->RemoveSiteAccessRequest(tab_id, extension.id());
}

PermissionsManager::UserSiteSetting
ExtensionsToolbarUnitTest::GetUserSiteSetting(const GURL& url) {
  return permissions_manager_->GetUserSiteSetting(url::Origin::Create(url));
}

PermissionsManager::UserSiteAccess ExtensionsToolbarUnitTest::GetUserSiteAccess(
    const extensions::Extension& extension,
    const GURL& url) const {
  return permissions_manager_->GetUserSiteAccess(extension, url);
}

SitePermissionsHelper::SiteInteraction
ExtensionsToolbarUnitTest::GetSiteInteraction(
    const extensions::Extension& extension,
    content::WebContents* web_contents) const {
  return permissions_helper_->GetSiteInteraction(extension, web_contents);
}

std::vector<ToolbarActionView*>
ExtensionsToolbarUnitTest::GetPinnedExtensionViews() {
  std::vector<ToolbarActionView*> result;
  for (views::View* child : extensions_container()->children()) {
    // Ensure we don't downcast the ExtensionsToolbarButton.
    if (views::IsViewClass<ToolbarActionView>(child)) {
      ToolbarActionView* const action = static_cast<ToolbarActionView*>(child);
#if BUILDFLAG(IS_MAC)
      // TODO(crbug.com/40670141): Use IsActionVisibleOnToolbar() because it
      // queries the underlying model and not GetVisible(), as that relies on an
      // animation running, which is not reliable in unit tests on Mac.
      const bool is_visible = extensions_container()->IsActionVisibleOnToolbar(
          action->view_controller()->GetId());
#else
      const bool is_visible = action->GetVisible();
#endif
      if (is_visible)
        result.push_back(action);
    }
  }
  return result;
}

std::vector<std::string> ExtensionsToolbarUnitTest::GetPinnedExtensionNames() {
  return base::ToVector(GetPinnedExtensionViews(), [](ToolbarActionView* view) {
    return base::UTF16ToUTF8(view->view_controller()->GetActionName());
  });
}

void ExtensionsToolbarUnitTest::WaitForAnimation() {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40670141): we avoid using animations on Mac due to the lack
  // of support in unit tests. Therefore this is a no-op.
#else
  views::test::WaitForAnimatingLayoutManager(extensions_container());
#endif
}

void ExtensionsToolbarUnitTest::LayoutContainerIfNecessary() {
  extensions_container()->GetWidget()->LayoutRootViewIfNecessary();
}

content::WebContentsTester*
ExtensionsToolbarUnitTest::AddWebContentsAndGetTester() {
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* raw_contents = contents.get();
  browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), raw_contents);
  return content::WebContentsTester::For(raw_contents);
}

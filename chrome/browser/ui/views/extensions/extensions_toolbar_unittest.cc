// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"

#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view_utils.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

base::Value::List ToListValue(const std::vector<std::string>& permissions) {
  extensions::ListBuilder builder;
  for (const std::string& permission : permissions)
    builder.Append(permission);
  return builder.Build();
}

}  // namespace

void ExtensionsToolbarUnitTest::SetUp() {
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
          .AddPermissions(permissions)
          .SetManifestKey("host_permissions", ToListValue(host_permissions))
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
  extensions::PermissionsManagerWaiter waiter(
      extensions::PermissionsManager::Get(profile()));
  extensions::ScriptingPermissionsModifier(profile(), extension)
      .RemoveAllGrantedHostPermissions();
  waiter.WaitForExtensionPermissionsUpdate();
}

void ExtensionsToolbarUnitTest::ClickButton(views::Button* button) const {
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             0);
  button->OnMousePressed(press_event);
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
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
  SitePermissionsHelper(browser()->profile())
      .UpdateSiteAccess(extension, web_contents, site_access);
  waiter.WaitForExtensionPermissionsUpdate();
}

void ExtensionsToolbarUnitTest::UpdateUserSiteSetting(
    extensions::PermissionsManager::UserSiteSetting site_setting,
    const GURL& url) {
  auto* permissions_manager = PermissionsManager::Get(browser()->profile());
  extensions::PermissionsManagerWaiter waiter(permissions_manager);
  permissions_manager->UpdateUserSiteSetting(url::Origin::Create(url),
                                             site_setting);
  waiter.WaitForUserPermissionsSettingsChange();
}

PermissionsManager::UserSiteSetting
ExtensionsToolbarUnitTest::GetUserSiteSetting(const GURL& url) {
  return PermissionsManager::Get(browser()->profile())
      ->GetUserSiteSetting(url::Origin::Create(url));
}

std::vector<ToolbarActionView*>
ExtensionsToolbarUnitTest::GetPinnedExtensionViews() {
  std::vector<ToolbarActionView*> result;
  for (views::View* child : extensions_container()->children()) {
    // Ensure we don't downcast the ExtensionsToolbarButton.
    if (views::IsViewClass<ToolbarActionView>(child)) {
      ToolbarActionView* const action = static_cast<ToolbarActionView*>(child);
#if BUILDFLAG(IS_MAC)
      // TODO(crbug.com/1045212): Use IsActionVisibleOnToolbar() because it
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
  std::vector<ToolbarActionView*> views = GetPinnedExtensionViews();
  std::vector<std::string> result;
  result.resize(views.size());
  base::ranges::transform(views, result.begin(), [](ToolbarActionView* view) {
    return base::UTF16ToUTF8(view->view_controller()->GetActionName());
  });
  return result;
}

void ExtensionsToolbarUnitTest::WaitForAnimation() {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/1045212): we avoid using animations on Mac due to the lack
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

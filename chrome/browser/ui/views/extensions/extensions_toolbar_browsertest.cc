// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_browsertest.h"

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/host_access_request_helper.h"
#include "extensions/browser/permissions/scripting_permissions_modifier.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view_utils.h"

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

ExtensionsToolbarBrowserTest::ExtensionsToolbarBrowserTest() {
  // Allow unpacked extensions without developer mode for testing.
  scoped_feature_list_.InitAndDisableFeature(
      extensions_features::kExtensionDisableUnsupportedDeveloper);
}

ExtensionsToolbarBrowserTest::~ExtensionsToolbarBrowserTest() = default;

void ExtensionsToolbarBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  cooldown_reset_.emplace(
      extensions::HostAccessRequestsHelper::SetCooldownForTesting(
          base::TimeDelta()));

  extension_service_ =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  permissions_manager_ = PermissionsManager::Get(profile());
  permissions_helper_ = std::make_unique<SitePermissionsHelper>(profile());

  // Shorten delay on animations so tests run faster.
  views::test::ReduceAnimationDuration(extensions_container());
}

void ExtensionsToolbarBrowserTest::TearDownOnMainThread() {
  cooldown_reset_.reset();
  permissions_helper_.reset();
  extension_service_ = nullptr;
  permissions_manager_ = nullptr;

  InProcessBrowserTest::TearDownOnMainThread();
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarBrowserTest::InstallExtension(const std::string& name) {
  return InstallExtension(name, {}, {});
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarBrowserTest::InstallExtensionWithHostPermissions(
    const std::string& name,
    const std::vector<std::string>& host_permissions) {
  return InstallExtension(name, {}, host_permissions);
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarBrowserTest::InstallExtensionWithPermissions(
    const std::string& name,
    const std::vector<std::string>& permissions) {
  return InstallExtension(name, permissions, {});
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarBrowserTest::InstallEnterpriseExtension(
    const std::string& name,
    const std::vector<std::string>& host_permissions) {
  return InstallExtension(name, {}, host_permissions,
                          extensions::mojom::ManifestLocation::kExternalPolicy);
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarBrowserTest::InstallExtension(
    const std::string& name,
    const std::vector<std::string>& permissions,
    const std::vector<std::string>& host_permissions,
    extensions::mojom::ManifestLocation location) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetLocation(location)
          .AddAPIPermissions(permissions)
          .AddHostPermissions(host_permissions)
          .SetID(crx_file::id_util::GenerateId(name))
          .Build();
  extension_registrar()->AddExtension(extension);

  // Force the container to re-layout, since a new extension was added.
  LayoutContainerIfNecessary();

  return extension;
}

void ExtensionsToolbarBrowserTest::ReloadExtension(
    const extensions::ExtensionId& extension_id) {
  extension_registrar()->ReloadExtension(extension_id);
}

void ExtensionsToolbarBrowserTest::UninstallExtension(
    const extensions::ExtensionId& extension_id) {
  base::RunLoop run_loop;
  extension_registrar()->UninstallExtension(
      extension_id, extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING,
      nullptr, run_loop.QuitClosure());
  run_loop.Run();
}

void ExtensionsToolbarBrowserTest::EnableExtension(
    const extensions::ExtensionId& extension_id) {
  extension_registrar()->EnableExtension(extension_id);
}

void ExtensionsToolbarBrowserTest::DisableExtension(
    const extensions::ExtensionId& extension_id) {
  extension_registrar()->DisableExtension(
      extension_id, {extensions::disable_reason::DISABLE_USER_ACTION});
}

void ExtensionsToolbarBrowserTest::WithholdHostPermissions(
    const extensions::Extension* extension) {
  extensions::PermissionsManagerWaiter waiter(permissions_manager_);
  extensions::ScriptingPermissionsModifier(profile(), extension)
      .RemoveAllGrantedHostPermissions();
  waiter.WaitForExtensionPermissionsUpdate();
}

void ExtensionsToolbarBrowserTest::ClickButton(views::Button* button) const {
  if (button->size().IsEmpty()) {
    button->SetSize(button->GetPreferredSize());
  }
  gfx::Point center = button->GetLocalBounds().CenterPoint();
  ui::MouseEvent press_event(ui::EventType::kMousePressed, center,
                             center, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  ui::MouseEvent release_event(ui::EventType::kMouseReleased, center,
                               center, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
}

void ExtensionsToolbarBrowserTest::UpdateUserSiteAccess(
    const extensions::Extension& extension,
    content::WebContents* web_contents,
    PermissionsManager::UserSiteAccess site_access) {
  extensions::PermissionsManagerWaiter waiter(
      PermissionsManager::Get(browser()->GetProfile()));
  permissions_helper_->UpdateSiteAccess(
      extension, web_contents, site_access,
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  waiter.WaitForExtensionPermissionsUpdate();
}

void ExtensionsToolbarBrowserTest::UpdateUserSiteSetting(
    extensions::PermissionsManager::UserSiteSetting site_setting,
    const GURL& url) {
  extensions::PermissionsManagerWaiter waiter(permissions_manager_);
  permissions_manager_->UpdateUserSiteSetting(url::Origin::Create(url),
                                              site_setting);
  waiter.WaitForUserPermissionsSettingsChange();
}

void ExtensionsToolbarBrowserTest::AddHostAccessRequest(
    const extensions::Extension& extension,
    content::WebContents* web_contents,
    const std::optional<URLPattern>& filter) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  permissions_manager_->AddHostAccessRequest(web_contents, tab_id, extension,
                                             filter);
}

void ExtensionsToolbarBrowserTest::RemoveHostAccessRequest(
    const extensions::Extension& extension,
    content::WebContents* web_contents) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  permissions_manager_->RemoveHostAccessRequest(tab_id, extension.id());
}

PermissionsManager::UserSiteSetting
ExtensionsToolbarBrowserTest::GetUserSiteSetting(const GURL& url) {
  return permissions_manager_->GetUserSiteSetting(url::Origin::Create(url));
}

PermissionsManager::UserSiteAccess
ExtensionsToolbarBrowserTest::GetUserSiteAccess(
    const extensions::Extension& extension,
    const GURL& url) const {
  return permissions_manager_->GetUserSiteAccess(extension, url);
}

SitePermissionsHelper::SiteInteraction
ExtensionsToolbarBrowserTest::GetSiteInteraction(
    const extensions::Extension& extension,
    content::WebContents* web_contents) const {
  return permissions_helper_->GetSiteInteraction(extension, web_contents);
}

std::vector<ToolbarActionView*>
ExtensionsToolbarBrowserTest::GetPinnedExtensionViews() {
  std::vector<ToolbarActionView*> result;
  for (views::View* child : extensions_container()->children()) {
    if (views::IsViewClass<ToolbarActionView>(child)) {
      ToolbarActionView* const action = static_cast<ToolbarActionView*>(child);
#if BUILDFLAG(IS_MAC)
      // TODO(crbug.com/40670141): Use IsActionVisibleOnToolbar() because it
      // queries the underlying model and not GetVisible(), as that relies on an
      // animation running, which is not reliable on Mac.
      const bool is_visible = extensions_container()->IsActionVisibleOnToolbar(
          action->view_model()->GetId());
#else
      const bool is_visible = action->GetVisible();
#endif
      if (is_visible) {
        result.push_back(action);
      }
    }
  }
  return result;
}

std::vector<std::string>
ExtensionsToolbarBrowserTest::GetPinnedExtensionNames() {
  return base::ToVector(GetPinnedExtensionViews(), [](ToolbarActionView* view) {
    return base::UTF16ToUTF8(view->view_model()->GetActionName());
  });
}

void ExtensionsToolbarBrowserTest::WaitForAnimation() {
#if BUILDFLAG(IS_MAC)
  // No-op on Mac.
#else
  views::test::WaitForAnimatingLayoutManager(extensions_container());
#endif
}

void ExtensionsToolbarBrowserTest::LayoutContainerIfNecessary() {
  extensions_container()->GetWidget()->LayoutRootViewIfNecessary();
}

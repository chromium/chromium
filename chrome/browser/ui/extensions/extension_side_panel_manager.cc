// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_side_panel_manager.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_side_panel_coordinator.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

DEFINE_USER_DATA(ExtensionSidePanelManager);

ExtensionSidePanelManager::ExtensionSidePanelManager(
    BrowserWindowInterface* browser,
    SidePanelRegistry* registry)
    : profile_(browser->GetProfile()),
      browser_(browser),
      tab_interface_(nullptr),
      registry_(registry),
      for_tab_(false),
      scoped_unowned_user_data_(std::in_place,
                                browser->GetUnownedUserDataHost(),
                                *this) {
  RegisterExtensionEntries();
}

ExtensionSidePanelManager::ExtensionSidePanelManager(
    Profile* profile,
    tabs::TabInterface* tab_interface,
    SidePanelRegistry* tab_registry)
    : profile_(profile),
      browser_(nullptr),
      tab_interface_(tab_interface),
      registry_(tab_registry),
      for_tab_(true) {
  RegisterExtensionEntries();
}

ExtensionSidePanelManager::~ExtensionSidePanelManager() = default;

// static
ExtensionSidePanelManager* ExtensionSidePanelManager::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

ExtensionSidePanelCoordinator*
ExtensionSidePanelManager::GetExtensionCoordinatorForTesting(
    const ExtensionId& extension_id) {
  auto it = coordinators_.find(extension_id);
  return (it == coordinators_.end()) ? nullptr : it->second.get();
}

void ExtensionSidePanelManager::RegisterExtensionEntries() {
  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  extension_registry_observation_.Observe(extension_registry);

  for (const auto& extension : extension_registry->enabled_extensions()) {
    MaybeCreateExtensionSidePanelCoordinator(extension.get());
  }
}

void ExtensionSidePanelManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  MaybeCreateExtensionSidePanelCoordinator(extension);
}

void ExtensionSidePanelManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  auto it = coordinators_.find(extension->id());
  if (it != coordinators_.end()) {
    it->second->DeregisterEntry();
    coordinators_.erase(extension->id());
  }
}

void ExtensionSidePanelManager::MaybeCreateExtensionSidePanelCoordinator(
    const Extension* extension) {
  if (extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kSidePanel)) {
    coordinators_.emplace(extension->id(),
                          std::make_unique<ExtensionSidePanelCoordinator>(
                              profile_, browser_, tab_interface_, extension,
                              registry_, for_tab_));
  }
}

}  // namespace extensions

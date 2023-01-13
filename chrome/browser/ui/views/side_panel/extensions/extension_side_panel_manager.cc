// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/common/extensions/api/side_panel/side_panel_info.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

ExtensionSidePanelManager::ExtensionSidePanelManager(Browser* browser)
    : BrowserUserData<ExtensionSidePanelManager>(*browser), browser_(browser) {}

ExtensionSidePanelManager::~ExtensionSidePanelManager() = default;

ExtensionSidePanelCoordinator*
ExtensionSidePanelManager::GetExtensionCoordinatorForTesting(
    const ExtensionId& extension_id) {
  auto it = coordinators_.find(extension_id);
  return (it == coordinators_.end()) ? nullptr : it->second.get();
}

void ExtensionSidePanelManager::RegisterExtensionEntries(
    SidePanelRegistry* global_registry) {
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_->profile());
  extension_registry_observation_.Observe(extension_registry);

  for (const auto& extension : extension_registry->enabled_extensions()) {
    MaybeCreateExtensionSidePanelCoordinator(extension.get(), global_registry);
  }
}

void ExtensionSidePanelManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  MaybeCreateExtensionSidePanelCoordinator(
      extension, SidePanelCoordinator::GetGlobalSidePanelRegistry(browser_));
}

void ExtensionSidePanelManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  coordinators_.erase(extension->id());
}

void ExtensionSidePanelManager::MaybeCreateExtensionSidePanelCoordinator(
    const Extension* extension,
    SidePanelRegistry* global_registry) {
  if (extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kSidePanel)) {
    coordinators_.emplace(extension->id(),
                          std::make_unique<ExtensionSidePanelCoordinator>(
                              browser_, extension, global_registry));
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ExtensionSidePanelManager);

}  // namespace extensions

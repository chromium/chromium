// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace {

// The user data key used to store the ExtensionSidePanelManager for a browser.
const char kExtensionSidePanelManagerKey[] = "extension_side_panel_manager";

}  // namespace

ExtensionSidePanelManager::ExtensionSidePanelManager(
    Browser* browser,
    SidePanelRegistry* global_registry)
    : browser_(browser), global_registry_(global_registry) {
  side_panel_registry_observation_.Observe(global_registry_);
}

ExtensionSidePanelManager::~ExtensionSidePanelManager() = default;

// static
ExtensionSidePanelManager* ExtensionSidePanelManager::GetOrCreateForBrowser(
    Browser* browser) {
  ExtensionSidePanelManager* manager = static_cast<ExtensionSidePanelManager*>(
      browser->GetUserData(kExtensionSidePanelManagerKey));
  if (!manager) {
    // Use absl::WrapUnique(new ExtensionSidePanelManager(...)) instead of
    // std::make_unique<ExtensionSidePanelManager> to access a private
    // constructor.
    auto new_manager = absl::WrapUnique(new ExtensionSidePanelManager(
        browser, SidePanelCoordinator::GetGlobalSidePanelRegistry(browser)));
    manager = new_manager.get();
    browser->SetUserData(kExtensionSidePanelManagerKey, std::move(new_manager));
  }
  return manager;
}

ExtensionSidePanelCoordinator*
ExtensionSidePanelManager::GetExtensionCoordinatorForTesting(
    const ExtensionId& extension_id) {
  auto it = coordinators_.find(extension_id);
  return (it == coordinators_.end()) ? nullptr : it->second.get();
}

void ExtensionSidePanelManager::RegisterExtensionEntries() {
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_->profile());
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
  coordinators_.erase(extension->id());
}

void ExtensionSidePanelManager::OnRegistryDestroying(
    SidePanelRegistry* registry) {
  coordinators_.clear();
  side_panel_registry_observation_.Reset();
}

void ExtensionSidePanelManager::MaybeCreateExtensionSidePanelCoordinator(
    const Extension* extension) {
  if (extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kSidePanel)) {
    coordinators_.emplace(extension->id(),
                          std::make_unique<ExtensionSidePanelCoordinator>(
                              browser_, extension, global_registry_));
  }
}

}  // namespace extensions

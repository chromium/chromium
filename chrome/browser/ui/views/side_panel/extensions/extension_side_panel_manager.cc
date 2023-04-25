// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
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
    Profile* profile,
    Browser* browser,
    content::WebContents* web_contents,
    SidePanelRegistry* registry)
    : profile_(profile),
      browser_(browser),
      web_contents_(web_contents),
      registry_(registry) {
  side_panel_registry_observation_.Observe(registry_);
  profile_observation_.Observe(profile);
  RegisterExtensionEntries();
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
        browser->profile(), browser, /*web_contents=*/nullptr,
        SidePanelCoordinator::GetGlobalSidePanelRegistry(browser)));
    manager = new_manager.get();
    browser->SetUserData(kExtensionSidePanelManagerKey, std::move(new_manager));
  }
  return manager;
}

// static
ExtensionSidePanelManager* ExtensionSidePanelManager::GetOrCreateForWebContents(
    Profile* profile,
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  ExtensionSidePanelManager* manager = static_cast<ExtensionSidePanelManager*>(
      web_contents->GetUserData(kExtensionSidePanelManagerKey));
  if (!manager) {
    // Use absl::WrapUnique(new ExtensionSidePanelManager(...)) instead of
    // std::make_unique<ExtensionSidePanelManager> to access a private
    // constructor.
    auto new_manager = absl::WrapUnique(new ExtensionSidePanelManager(
        profile, /*browser=*/nullptr, web_contents,
        SidePanelRegistry::Get(web_contents)));
    manager = new_manager.get();
    web_contents->SetUserData(kExtensionSidePanelManagerKey,
                              std::move(new_manager));
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
  coordinators_.erase(extension->id());
}

void ExtensionSidePanelManager::OnRegistryDestroying(
    SidePanelRegistry* registry) {
  coordinators_.clear();
  side_panel_registry_observation_.Reset();
  registry_ = nullptr;
}

void ExtensionSidePanelManager::OnProfileWillBeDestroyed(Profile* profile) {
  // Destroy all coordinators, since no functionality should remain once there's
  // no profile.
  coordinators_.clear();

  CHECK_EQ(profile_, profile);
  profile_observation_.Reset();
  profile_ = nullptr;
}

void ExtensionSidePanelManager::MaybeCreateExtensionSidePanelCoordinator(
    const Extension* extension) {
  if (extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kSidePanel)) {
    coordinators_.emplace(
        extension->id(),
        std::make_unique<ExtensionSidePanelCoordinator>(
            profile_, browser_, web_contents_, extension, registry_));
  }
}

}  // namespace extensions

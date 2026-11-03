// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/context_hub/memory_banks_hotkey_manager_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/views/context_hub/memory_banks_hotkey_manager.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"

namespace {

bool IsHotkeyManagerEnabled() {
  return base::FeatureList::IsEnabled(context_hub::features::kMemoryBanks) &&
         base::FeatureList::IsEnabled(
             browser::context_hub::mojom::kMemoryBanksSearchPalette);
}

}  // namespace

// static
MemoryBanksHotkeyManager* MemoryBanksHotkeyManagerFactory::GetForProfile(
    Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  return static_cast<MemoryBanksHotkeyManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
MemoryBanksHotkeyManagerFactory*
MemoryBanksHotkeyManagerFactory::GetInstance() {
  static base::NoDestructor<MemoryBanksHotkeyManagerFactory> instance;
  return instance.get();
}

MemoryBanksHotkeyManagerFactory::MemoryBanksHotkeyManagerFactory()
    : ProfileKeyedServiceFactory("MemoryBanksHotkeyManager",
                                 ProfileSelections::BuildForRegularProfile()) {
  // The manager observes the profile's `ProfileBrowserCollection`, which is
  // provided by `BrowserManagerService`.
  DependsOn(BrowserManagerServiceFactory::GetInstance());
}

MemoryBanksHotkeyManagerFactory::~MemoryBanksHotkeyManagerFactory() = default;

std::unique_ptr<KeyedService>
MemoryBanksHotkeyManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!IsHotkeyManagerEnabled()) {
    return nullptr;
  }
  return std::make_unique<MemoryBanksHotkeyManager>(
      CHECK_DEREF(Profile::FromBrowserContext(context)));
}

bool MemoryBanksHotkeyManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Creates the manager when the profile loads instead of on first use, since
  // nothing else asks for it and the shortcut has to work right away. Only when
  // the flags are on, so profiles do no work at startup otherwise.
  return IsHotkeyManagerEnabled();
}

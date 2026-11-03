// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_HOTKEY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_HOTKEY_MANAGER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class MemoryBanksHotkeyManager;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

// Creates a `MemoryBanksHotkeyManager` for each regular profile when the
// Memory Banks search palette is enabled.
class MemoryBanksHotkeyManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the manager for `profile`, or nullptr if the feature is disabled or
  // `profile` is incognito.
  static MemoryBanksHotkeyManager* GetForProfile(Profile* profile);
  static MemoryBanksHotkeyManagerFactory* GetInstance();

  MemoryBanksHotkeyManagerFactory(const MemoryBanksHotkeyManagerFactory&) =
      delete;
  MemoryBanksHotkeyManagerFactory& operator=(
      const MemoryBanksHotkeyManagerFactory&) = delete;

 private:
  friend base::NoDestructor<MemoryBanksHotkeyManagerFactory>;

  MemoryBanksHotkeyManagerFactory();
  ~MemoryBanksHotkeyManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_HOTKEY_MANAGER_FACTORY_H_

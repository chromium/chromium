// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_HOTKEY_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_HOTKEY_MANAGER_H_

#include "base/functional/function_ref.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

class BrowserCollection;
class BrowserWindowInterface;
class Profile;

namespace views {
class FocusManager;
}  // namespace views

// Registers the user-configured Memory Banks keyboard shortcut (stored in
// `context_hub::prefs::kContextHubBrowseMemoryBanksShortcut`) with every
// tabbed browser window of a profile, and opens the Memory Banks search
// palette on the active tab when the shortcut is pressed. No shortcut is
// registered by default.
class MemoryBanksHotkeyManager : public KeyedService,
                                 public ui::AcceleratorTarget,
                                 public BrowserCollectionObserver {
 public:
  explicit MemoryBanksHotkeyManager(Profile& profile);
  MemoryBanksHotkeyManager(const MemoryBanksHotkeyManager&) = delete;
  MemoryBanksHotkeyManager& operator=(const MemoryBanksHotkeyManager&) = delete;
  ~MemoryBanksHotkeyManager() override;

  // KeyedService:
  void Shutdown() override;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // Returns the currently registered shortcut, or an empty accelerator if none.
  const ui::Accelerator& accelerator_for_testing() const {
    return accelerator_;
  }

 private:
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // Re-reads the shortcut pref and moves the registration from the old
  // shortcut to the new one on every window.
  void UpdateRegistration();

  // Adds or removes the current shortcut on one window. Does nothing if no
  // shortcut is set.
  void RegisterShortcut(views::FocusManager& focus_manager);
  void UnregisterShortcut(views::FocusManager& focus_manager);

  // Runs `fn` on the `FocusManager` of every tabbed window of the profile.
  void ForEachFocusManager(base::FunctionRef<void(views::FocusManager&)> fn);

  const raw_ref<Profile> profile_;

  // The shortcut registered on every window. Empty when none is set.
  ui::Accelerator accelerator_;

  // Watches the shortcut pref, so changes made in Settings apply right away.
  PrefChangeRegistrar pref_change_registrar_;

  // Watches the profile's windows, so new windows also get the shortcut.
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_HOTKEY_MANAGER_H_

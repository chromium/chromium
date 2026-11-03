// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/context_hub/memory_banks_hotkey_manager.h"

#include "base/functional/bind.h"
#include "chrome/browser/context_hub/memory_banks_shortcut.h"
#include "chrome/browser/context_hub/prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/views/context_hub/memory_banks_palette_bubble_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/views/focus/focus_manager.h"

namespace {

// Returns the `FocusManager` of `browser` if it is a normal tabbed window.
// Popups, app windows and DevTools windows don't get the shortcut.
views::FocusManager* GetFocusManager(BrowserWindowInterface* browser) {
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return nullptr;
  }
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view ? browser_view->GetFocusManager() : nullptr;
}

}  // namespace

MemoryBanksHotkeyManager::MemoryBanksHotkeyManager(Profile& profile)
    : profile_(profile) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      context_hub::prefs::kContextHubBrowseMemoryBanksShortcut,
      base::BindRepeating(&MemoryBanksHotkeyManager::UpdateRegistration,
                          base::Unretained(this)));

  if (ProfileBrowserCollection* const collection =
          ProfileBrowserCollection::GetForProfile(&profile_.get())) {
    browser_collection_observation_.Observe(collection);
  }

  UpdateRegistration();
}

MemoryBanksHotkeyManager::~MemoryBanksHotkeyManager() = default;

void MemoryBanksHotkeyManager::Shutdown() {
  ForEachFocusManager([this](views::FocusManager& focus_manager) {
    UnregisterShortcut(focus_manager);
  });
  browser_collection_observation_.Reset();
  pref_change_registrar_.RemoveAll();
}

bool MemoryBanksHotkeyManager::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (!browser_collection_observation_.IsObserving()) {
    return false;
  }
  // The shortcut is only registered on tabbed windows, so the window it was
  // pressed in is the profile's most recently active tabbed window.
  BrowserWindowInterface* browser = nullptr;
  browser_collection_observation_.GetSource()->ForEach(
      [&browser](BrowserWindowInterface* candidate) {
        if (!GetFocusManager(candidate)) {
          return true;  // Skip popups and app windows.
        }
        browser = candidate;
        return false;  // Stop at the most recently active tabbed window.
      },
      BrowserCollection::Order::kActivation);
  tabs::TabInterface* const tab =
      browser ? browser->GetActiveTabInterface() : nullptr;
  if (!tab) {
    return false;
  }
  MemoryBanksPaletteBubbleController::GetOrCreateForWebContents(
      tab->GetContents())
      ->ShowBubble();
  return true;
}

bool MemoryBanksHotkeyManager::CanHandleAccelerators() const {
  // The shortcut is only registered while it is set, so it can always run.
  return true;
}

void MemoryBanksHotkeyManager::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (views::FocusManager* const focus_manager = GetFocusManager(browser)) {
    RegisterShortcut(*focus_manager);
  }
}

void MemoryBanksHotkeyManager::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  // Unregister as soon as the window starts closing. After that,
  // `ForEachFocusManager()` no longer sees it, so later shortcut changes and
  // the destructor can't reach it.
  if (views::FocusManager* const focus_manager = GetFocusManager(browser)) {
    UnregisterShortcut(*focus_manager);
  }
}

void MemoryBanksHotkeyManager::UpdateRegistration() {
  const ui::Accelerator new_accelerator =
      context_hub::GetBrowseMemoryBanksShortcutFromPrefs(*profile_->GetPrefs());
  if (new_accelerator == accelerator_) {
    return;
  }
  ForEachFocusManager([this](views::FocusManager& focus_manager) {
    UnregisterShortcut(focus_manager);
  });
  accelerator_ = new_accelerator;
  ForEachFocusManager([this](views::FocusManager& focus_manager) {
    RegisterShortcut(focus_manager);
  });
}

void MemoryBanksHotkeyManager::RegisterShortcut(
    views::FocusManager& focus_manager) {
  if (!accelerator_.IsEmpty()) {
    focus_manager.RegisterAccelerator(
        accelerator_, ui::AcceleratorManager::kNormalPriority, this);
  }
}

void MemoryBanksHotkeyManager::UnregisterShortcut(
    views::FocusManager& focus_manager) {
  if (!accelerator_.IsEmpty()) {
    focus_manager.UnregisterAccelerator(accelerator_, this);
  }
}

void MemoryBanksHotkeyManager::ForEachFocusManager(
    base::FunctionRef<void(views::FocusManager&)> fn) {
  if (!browser_collection_observation_.IsObserving()) {
    return;
  }
  browser_collection_observation_.GetSource()->ForEach(
      [&fn](BrowserWindowInterface* browser) {
        if (views::FocusManager* const focus_manager =
                GetFocusManager(browser)) {
          fn(*focus_manager);
        }
        return true;  // Keep going through all windows.
      });
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_LOCKED_STATE_LOCKED_STATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_CHROMEOS_LOCKED_STATE_LOCKED_STATE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace chromeos {

enum class LockedState {
  // Browser is not locked.
  kUnlocked,
  // Browser is locked by an extension (legacy locked fullscreen / quiz mode).
  kExtensionLocked,
  // Browser is prepared for OnTask (managed window) but not physically
  // locked/pinned yet.
  kOnTaskPrepared,
  // Browser is locked for OnTask (physically pinned).
  kOnTaskLocked,
  // Browser is locked for OnTask but paused (physically pinned, but immersive
  // mode disabled).
  kOnTaskLockedPaused,
};

enum class ContextMenuPolicy {
  kBlockAll,  // Default: All context menu items disabled.
  kLimited,   // Limited: Allows navigation, copy image, custom extension
              // commands, etc. See `LockedStateController::IsCommandIdEnabled`
              // for more detail.
  kAllowAll,  // Allow all: Context menu behaves normally.
};

struct LockedStateCapabilities {
  bool supports_tab_strip = true;  // Allow tab strip to be visible/supported
  // TODO(crbug.com/438540029): Consider allowing this.
  bool allow_find = true;  // Allow Find in Page (IDC_FIND)
  bool allow_browser_navigation =
      true;  // Allow navigation (blocked at navigator level if false)
  bool allow_tab_modification =
      true;  // Allow modifying tabs (closing, dragging, detaching, reordering)
  // TODO(crbug.com/438540029): Move capabilities only used in
  // LockedStateController (e.g., use_immersive_mode, focus_toolbar_on_lock) to
  // internal state.
  bool use_immersive_mode = false;  // Enable immersive mode to show frame/tabs
  bool focus_toolbar_on_lock =
      false;  // Focus the toolbar immediately after locking
  ContextMenuPolicy context_menu_policy = ContextMenuPolicy::kAllowAll;

  bool operator==(const LockedStateCapabilities& other) const = default;
};

// Manages the locked state (unlocked, prepared for OnTask, extension locked,
// OnTask locked) of a browser window and restricts capabilities/commands
// based on that state.
class LockedStateController {
 public:
  DECLARE_USER_DATA(LockedStateController);

  static LockedStateController* From(
      BrowserWindowInterface* browser_window_interface);

  enum class CommandType {
    kUnknown,
    kClipboard,              // IDC_CUT, IDC_COPY, IDC_PASTE
    kPageNavigation,         // IDC_BACK, IDC_FORWARD, IDC_RELOAD
    kAllowedContentContext,  // IDC_CONTENT_CONTEXT_COPYIMAGE, COPYIMAGELOCATION
    kExtensionsCustom,       // Extensions Custom commands
    kTabManagement,          // Select next/prev/specific tab
    kFindInPage,             // Find, FindNext, FindPrevious
  };

  explicit LockedStateController(
      BrowserWindowInterface* browser_window_interface);
  ~LockedStateController();

  LockedStateController(const LockedStateController&) = delete;
  LockedStateController& operator=(const LockedStateController&) = delete;

  // Try to lock the browser into specified state. Capabilities are determined
  // internally. Fails if already locked with a different state (transitions
  // between kOnTaskLocked and kOnTaskLockedPaused are allowed).
  bool Lock(LockedState state);

  // Unlock the browser. Fails if expected_state does not match current state.
  // TODO(oshima): Consider removing the `expected_state` argument when
  // stabilized.
  bool Unlock(LockedState expected_state);

  // Returns true if the browser is physically locked/pinned (ExtensionLocked,
  // OnTaskLocked, OnTaskLockedPaused).
  bool IsLocked() const;

  // Returns true if the browser is in extension locked fullscreen.
  bool IsLockedFullscreen() const;

  LockedState GetState() const;
  const LockedStateCapabilities& GetCapabilities() const;

  // Returns true if the specified command ID is allowed in the current locked
  // state.
  bool IsCommandIdEnabled(int command_id) const;

  // Returns true if the specified command ID's update should be blocked.
  bool IsCommandUpdateBlocked(int command_id) const;

  // Returns true if the FindBar should be visible in the current locked state.
  bool CanShowFindBar() const;

  // Returns true if the browser is in any OnTask state (Prepared, Locked,
  // LockedPaused).
  bool IsLockedForOnTask() const;

 private:
  void UpdateCapabilities();
  void NotifyStateUpdated();

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  LockedState state_ = LockedState::kUnlocked;
  LockedStateCapabilities capabilities_;
  ui::ScopedUnownedUserData<LockedStateController> scoped_unowned_user_data_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_LOCKED_STATE_LOCKED_STATE_CONTROLLER_H_

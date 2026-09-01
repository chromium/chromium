// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/locked_state/locked_state_controller.h"

#include "ash/wm/window_pin_util.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/common/chrome_features.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace chromeos {

DEFINE_USER_DATA(LockedStateController);

namespace {
// Default unrestricted capabilities for unlocked state.
constexpr LockedStateCapabilities kUnlockedCapabilities = {
    .supports_tab_strip = true,
    .allow_find = true,
    .allow_browser_navigation = true,
    .allow_tab_modification = true,
    .use_immersive_mode = false,
    .focus_toolbar_on_lock = false,
    .context_menu_policy = ContextMenuPolicy::kAllowAll,
};

// Restricted capabilities for extension-locked fullscreen (e.g. quiz mode).
constexpr LockedStateCapabilities kExtensionLockedCapabilities = {
    .supports_tab_strip = false,
    .allow_find = false,
    .allow_browser_navigation = false,
    .allow_tab_modification = false,
    .use_immersive_mode = false,
    .focus_toolbar_on_lock = false,
    .context_menu_policy = ContextMenuPolicy::kBlockAll,
};

// Default baseline capabilities for OnTask. Some fields (like tab strip and
// immersive mode) will be dynamically updated based on window features.
constexpr LockedStateCapabilities kDefaultOnTaskCapabilities = {
    .supports_tab_strip = false,
    .allow_find = true,
    .allow_browser_navigation = true,
    .allow_tab_modification = false,
    .use_immersive_mode = false,
    .focus_toolbar_on_lock = false,
    .context_menu_policy = ContextMenuPolicy::kLimited,
};

bool IsTrustedPinnedState(LockedState state) {
  return state == LockedState::kExtensionLocked ||
         state == LockedState::kOnTaskLocked ||
         state == LockedState::kOnTaskLockedPaused;
}

constexpr auto kCommandMap =
    base::MakeFixedFlatMap<int, LockedStateController::CommandType>({
        {IDC_CUT, LockedStateController::CommandType::kClipboard},
        {IDC_COPY, LockedStateController::CommandType::kClipboard},
        {IDC_PASTE, LockedStateController::CommandType::kClipboard},
        {IDC_BACK, LockedStateController::CommandType::kPageNavigation},
        {IDC_FORWARD, LockedStateController::CommandType::kPageNavigation},
        {IDC_RELOAD, LockedStateController::CommandType::kPageNavigation},
        {IDC_RELOAD_BYPASSING_CACHE,
         LockedStateController::CommandType::kPageNavigation},
        {IDC_RELOAD_CLEARING_CACHE,
         LockedStateController::CommandType::kPageNavigation},
        {IDC_STOP, LockedStateController::CommandType::kPageNavigation},
        {IDC_CONTENT_CONTEXT_COPYIMAGE,
         LockedStateController::CommandType::kAllowedContentContext},
        {IDC_CONTENT_CONTEXT_COPYIMAGELOCATION,
         LockedStateController::CommandType::kAllowedContentContext},
        {IDC_SELECT_NEXT_TAB,
         LockedStateController::CommandType::kTabManagement},
        {IDC_SELECT_PREVIOUS_TAB,
         LockedStateController::CommandType::kTabManagement},
        {IDC_SELECT_TAB_0, LockedStateController::CommandType::kTabManagement},
        {IDC_SELECT_TAB_1, LockedStateController::CommandType::kTabManagement},
        {IDC_SELECT_TAB_2, LockedStateController::CommandType::kTabManagement},
        {IDC_SELECT_TAB_3, LockedStateController::CommandType::kTabManagement},
        {IDC_SELECT_TAB_4, LockedStateController::CommandType::kTabManagement},
        {IDC_SELECT_TAB_5, LockedStateController::CommandType::kTabManagement},
        {IDC_SELECT_TAB_6, LockedStateController::CommandType::kTabManagement},
        {IDC_SELECT_TAB_7, LockedStateController::CommandType::kTabManagement},
        {IDC_FIND, LockedStateController::CommandType::kFindInPage},
        {IDC_FIND_NEXT, LockedStateController::CommandType::kFindInPage},
        {IDC_FIND_PREVIOUS, LockedStateController::CommandType::kFindInPage},
        {IDC_CLOSE_FIND_OR_STOP,
         LockedStateController::CommandType::kFindInPage},
    });

// The range of command IDs reserved for extension custom menus.
constexpr int kMaxExtensionCustomCommands = 1000;
constexpr int IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST =
    IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + kMaxExtensionCustomCommands;

bool IsExtensionsCustomCommandId(int command_id) {
  return command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
         command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST;
}

LockedStateController::CommandType GetCommandType(int command_id) {
  const auto it = kCommandMap.find(command_id);
  if (it != kCommandMap.end()) {
    return it->second;
  }
  if (IsExtensionsCustomCommandId(command_id)) {
    return LockedStateController::CommandType::kExtensionsCustom;
  }
  return LockedStateController::CommandType::kUnknown;
}

}  // namespace

// static
LockedStateController* LockedStateController::From(
    BrowserWindowInterface* browser_window_interface) {
  if (features::IsUseUnifiedLockedStateControllerEnabled()) {
    return Get(browser_window_interface->GetUnownedUserDataHost());
  }
  return nullptr;
}

LockedStateController::LockedStateController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      capabilities_(kUnlockedCapabilities),
      scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {}

LockedStateController::~LockedStateController() = default;

bool LockedStateController::Lock(LockedState state) {
  CHECK_NE(state, LockedState::kUnlocked);

  if (state_ == state) {
    return true;  // No-op
  }

  // Transitions between OnTask locked and OnTask locked paused are allowed.
  // Also allow transitions from OnTask prepared to OnTask locked/paused.
  bool is_ontask_transition = (state_ == LockedState::kOnTaskPrepared ||
                               state_ == LockedState::kOnTaskLocked ||
                               state_ == LockedState::kOnTaskLockedPaused) &&
                              (state == LockedState::kOnTaskLocked ||
                               state == LockedState::kOnTaskLockedPaused);

  if (state_ != LockedState::kUnlocked && !is_ontask_transition) {
    LOG(WARNING)
        << "Cannot transition between different locked states directly: "
        << static_cast<int>(state_) << " -> " << static_cast<int>(state);
    return false;
  }

  bool was_pinned = IsTrustedPinnedState(state_);
  bool should_be_pinned = IsTrustedPinnedState(state);

  state_ = state;
  UpdateCapabilities();

  if (should_be_pinned && !was_pinned) {
    ash::PinWindow(browser_window_interface_->GetWindow()->GetNativeWindow(),
                   /*trusted=*/true);
    chrome::BrowserCommandController::From(browser_window_interface_)
        ->LockedFullscreenStateChanged();
    if (capabilities_.focus_toolbar_on_lock) {
      ToolbarButtonProvider::From(browser_window_interface_)->FocusToolbar();
    }
  } else if (!should_be_pinned && was_pinned) {
    ash::UnpinWindow(browser_window_interface_->GetWindow()->GetNativeWindow());
    chrome::BrowserCommandController::From(browser_window_interface_)
        ->LockedFullscreenStateChanged();
  }

  NotifyStateUpdated();
  return true;
}

bool LockedStateController::Unlock(LockedState expected_state) {
  if (state_ == LockedState::kUnlocked) {
    return true;  // Already unlocked
  }

  bool state_matches = (state_ == expected_state) ||
                       (expected_state == LockedState::kOnTaskLocked &&
                        (state_ == LockedState::kOnTaskLockedPaused ||
                         state_ == LockedState::kOnTaskPrepared));

  if (!state_matches) {
    LOG(WARNING) << "Rejecting unlock request for state "
                 << static_cast<int>(expected_state)
                 << " because current state is " << static_cast<int>(state_);
    return false;
  }

  bool was_pinned = IsTrustedPinnedState(state_);

  state_ = LockedState::kUnlocked;
  UpdateCapabilities();

  if (was_pinned) {
    ash::UnpinWindow(browser_window_interface_->GetWindow()->GetNativeWindow());
    chrome::BrowserCommandController::From(browser_window_interface_)
        ->LockedFullscreenStateChanged();
  }

  NotifyStateUpdated();
  return true;
}

bool LockedStateController::IsLocked() const {
  return IsTrustedPinnedState(state_);
}

bool LockedStateController::IsLockedFullscreen() const {
  return state_ == LockedState::kExtensionLocked;
}

bool LockedStateController::IsLockedForOnTask() const {
  return state_ == LockedState::kOnTaskPrepared ||
         state_ == LockedState::kOnTaskLocked ||
         state_ == LockedState::kOnTaskLockedPaused;
}

LockedState LockedStateController::GetState() const {
  return state_;
}

const LockedStateCapabilities& LockedStateController::GetCapabilities() const {
  return capabilities_;
}

bool LockedStateController::IsCommandIdEnabled(int command_id) const {
  if (state_ == LockedState::kUnlocked) {
    return true;
  }

  CommandType type = GetCommandType(command_id);
  switch (type) {
    case CommandType::kClipboard:
      return true;
    case CommandType::kTabManagement:
      return capabilities_.supports_tab_strip;
    case CommandType::kFindInPage:
      return capabilities_.allow_find;
    case CommandType::kPageNavigation:
      return capabilities_.allow_browser_navigation;
    case CommandType::kAllowedContentContext:
    case CommandType::kExtensionsCustom:
      return capabilities_.context_menu_policy != ContextMenuPolicy::kBlockAll;
    case CommandType::kUnknown:
      break;
  }

  // For other command IDs, follow the context menu policy.
  switch (capabilities_.context_menu_policy) {
    case ContextMenuPolicy::kBlockAll:
      return false;
    case ContextMenuPolicy::kAllowAll:
      return true;
    case ContextMenuPolicy::kLimited:
      return false;
  }
}

bool LockedStateController::IsCommandUpdateBlocked(int command_id) const {
  if (state_ == LockedState::kUnlocked) {
    return false;
  }
  CommandType type = GetCommandType(command_id);

  if ((type == CommandType::kTabManagement &&
       capabilities_.supports_tab_strip) ||
      (type == CommandType::kFindInPage && capabilities_.allow_find) ||
      (type == CommandType::kPageNavigation &&
       capabilities_.allow_browser_navigation) ||
      type == CommandType::kClipboard) {
    return false;
  }
  return true;
}

bool LockedStateController::CanShowFindBar() const {
  return capabilities_.allow_find;
}

void LockedStateController::UpdateCapabilities() {
  if (state_ == LockedState::kUnlocked) {
    capabilities_ = kUnlockedCapabilities;
    return;
  }

  if (state_ == LockedState::kExtensionLocked) {
    capabilities_ = kExtensionLockedCapabilities;
    return;
  }

  if (state_ == LockedState::kOnTaskPrepared ||
      state_ == LockedState::kOnTaskLocked ||
      state_ == LockedState::kOnTaskLockedPaused) {
    capabilities_ = kDefaultOnTaskCapabilities;
    bool supports_tabs =
        WindowFeatureController::From(browser_window_interface_)
            ->CanSupportWindowFeature(
                WindowFeatureController::WindowFeature::kFeatureTabStrip);
    capabilities_.supports_tab_strip = supports_tabs;

    if (state_ == LockedState::kOnTaskLocked ||
        state_ == LockedState::kOnTaskPrepared) {
      capabilities_.use_immersive_mode = supports_tabs;
      capabilities_.focus_toolbar_on_lock = supports_tabs;
    } else {
      // kOnTaskLockedPaused
      capabilities_.use_immersive_mode = false;
      capabilities_.focus_toolbar_on_lock = false;
    }
    return;
  }

  NOTREACHED();
}

void LockedStateController::NotifyStateUpdated() {
  aura::Window* native_window =
      browser_window_interface_->GetWindow()->GetNativeWindow();
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(native_window);
  bool locked = IsLocked();
  widget->widget_delegate()->SetCanMinimize(!locked);
  widget->widget_delegate()->SetShowCloseButton(!locked);
}

}  // namespace chromeos

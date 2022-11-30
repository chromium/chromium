// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/window_state_type.h"

#include "base/notreached.h"

namespace chromeos {

std::ostream& operator<<(std::ostream& stream, WindowStateType state) {
  switch (state) {
    case WindowStateType::kDefault:
      return stream << "kDefault";
    case WindowStateType::kNormal:
      return stream << "kNormal";
    case WindowStateType::kMinimized:
      return stream << "kMinimized";
    case WindowStateType::kMaximized:
      return stream << "kMaximized";
    case WindowStateType::kInactive:
      return stream << "kInactive";
    case WindowStateType::kFullscreen:
      return stream << "kFullscreen";
    case WindowStateType::kPrimarySnapped:
      return stream << "kLeftSnapped";
    case WindowStateType::kSecondarySnapped:
      return stream << "kRightSnapped";
    case WindowStateType::kPinned:
      return stream << "kPinned";
    case WindowStateType::kTrustedPinned:
      return stream << "kTrustedPinned";
    case WindowStateType::kPip:
      return stream << "kPip";
    case WindowStateType::kFloated:
      return stream << "kFloated";
  }

  NOTREACHED();
  return stream;
}

WindowStateType ToWindowStateType(ui::WindowShowState state) {
  switch (state) {
    case ui::SHOW_STATE_DEFAULT:
      return WindowStateType::kDefault;
    case ui::SHOW_STATE_NORMAL:
      return WindowStateType::kNormal;
    case ui::SHOW_STATE_MINIMIZED:
      return WindowStateType::kMinimized;
    case ui::SHOW_STATE_MAXIMIZED:
      return WindowStateType::kMaximized;
    case ui::SHOW_STATE_INACTIVE:
      return WindowStateType::kInactive;
    case ui::SHOW_STATE_FULLSCREEN:
      return WindowStateType::kFullscreen;
    case ui::SHOW_STATE_END:
      NOTREACHED();
      return WindowStateType::kDefault;
  }
}

ui::WindowShowState ToWindowShowState(WindowStateType type) {
  switch (type) {
    case WindowStateType::kDefault:
      return ui::SHOW_STATE_DEFAULT;
    case WindowStateType::kNormal:
    case WindowStateType::kSecondarySnapped:
    case WindowStateType::kPrimarySnapped:
    case WindowStateType::kPip:
    case WindowStateType::kFloated:
      return ui::SHOW_STATE_NORMAL;

    case WindowStateType::kMinimized:
      return ui::SHOW_STATE_MINIMIZED;
    case WindowStateType::kMaximized:
      return ui::SHOW_STATE_MAXIMIZED;
    case WindowStateType::kInactive:
      return ui::SHOW_STATE_INACTIVE;
    case WindowStateType::kFullscreen:
    case WindowStateType::kPinned:
    case WindowStateType::kTrustedPinned:
      return ui::SHOW_STATE_FULLSCREEN;
  }
  NOTREACHED();
  return ui::SHOW_STATE_DEFAULT;
}

bool IsPinnedWindowStateType(WindowStateType type) {
  return type == WindowStateType::kPinned ||
         type == WindowStateType::kTrustedPinned;
}

bool IsFullscreenOrPinnedWindowStateType(WindowStateType type) {
  return type == WindowStateType::kFullscreen || IsPinnedWindowStateType(type);
}

bool IsMaximizedOrFullscreenOrPinnedWindowStateType(WindowStateType type) {
  return type == WindowStateType::kMaximized ||
         IsFullscreenOrPinnedWindowStateType(type);
}

bool IsMinimizedWindowStateType(WindowStateType type) {
  return type == WindowStateType::kMinimized;
}

bool IsNormalWindowStateType(WindowStateType type) {
  return type == WindowStateType::kNormal || type == WindowStateType::kDefault;
}

bool IsSnappedWindowStateType(WindowStateType type) {
  return type == WindowStateType::kPrimarySnapped ||
         type == WindowStateType::kSecondarySnapped;
}

}  // namespace chromeos

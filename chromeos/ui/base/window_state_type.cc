// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/window_state_type.h"

#include "base/notreached.h"
#include "ui/base/mojom/window_show_state.mojom.h"

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

  NOTREACHED_IN_MIGRATION();
  return stream;
}

WindowStateType ToWindowStateType(ui::mojom::WindowShowState state) {
  switch (state) {
    case ui::mojom::WindowShowState::kDefault:
      return WindowStateType::kDefault;
    case ui::mojom::WindowShowState::kNormal:
      return WindowStateType::kNormal;
    case ui::mojom::WindowShowState::kMinimized:
      return WindowStateType::kMinimized;
    case ui::mojom::WindowShowState::kMaximized:
      return WindowStateType::kMaximized;
    case ui::mojom::WindowShowState::kInactive:
      return WindowStateType::kInactive;
    case ui::mojom::WindowShowState::kFullscreen:
      return WindowStateType::kFullscreen;
    case ui::mojom::WindowShowState::kEnd:
      NOTREACHED_IN_MIGRATION();
      return WindowStateType::kDefault;
  }
}

ui::mojom::WindowShowState ToWindowShowState(WindowStateType type) {
  switch (type) {
    case WindowStateType::kDefault:
      return ui::mojom::WindowShowState::kDefault;
    case WindowStateType::kNormal:
    case WindowStateType::kSecondarySnapped:
    case WindowStateType::kPrimarySnapped:
    case WindowStateType::kPip:
    case WindowStateType::kFloated:
      return ui::mojom::WindowShowState::kNormal;

    case WindowStateType::kMinimized:
      return ui::mojom::WindowShowState::kMinimized;
    case WindowStateType::kMaximized:
      return ui::mojom::WindowShowState::kMaximized;
    case WindowStateType::kInactive:
      return ui::mojom::WindowShowState::kInactive;
    case WindowStateType::kFullscreen:
    case WindowStateType::kPinned:
    case WindowStateType::kTrustedPinned:
      return ui::mojom::WindowShowState::kFullscreen;
  }
  NOTREACHED_IN_MIGRATION();
  return ui::mojom::WindowShowState::kDefault;
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

bool IsMaximizedOrFullscreenWindowStateType(WindowStateType type) {
  return type == WindowStateType::kMaximized ||
         type == WindowStateType::kFullscreen;
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

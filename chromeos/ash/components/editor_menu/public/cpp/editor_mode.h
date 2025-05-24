// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_MODE_H_
#define CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_MODE_H_

#include "base/component_export.h"

namespace chromeos::editor_menu {

enum class COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) EditorMode {
  // Blocked because it does not meet hard requirements such as user age,
  // country and policy.
  kHardBlocked,
  // Temporarily blocked because it does not meet transient requirements such as
  // internet connection, device mode.
  kSoftBlocked,
  // Mode that requires users to provide consent before using the feature.
  kConsentNeeded,
  // TODO: b:389553095 - With the introduction of EditorTextSelectionMode,
  // merge kRewrite and kWrite into a single enum value.
  // Feature in rewrite mode.
  kRewrite,
  // Feature in write mode.
  kWrite
};

}  // namespace chromeos::editor_menu

#endif  // CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_MODE_H_

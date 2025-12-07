// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_ENTERPRISE_POLICY_ENUMS_H_
#define CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_ENTERPRISE_POLICY_ENUMS_H_

#include "base/component_export.h"

namespace chromeos::editor_menu {

enum class EditorEnterprisePolicy : int {
  // Editor feature is allowed, and allows Google to use relevant data to
  // improve its AI models.
  kAllowedWithModelImprovement = 0,
  // Editor feature is allowed, however not allows Google to use relevant data
  // to improve its AI models.
  kAllowedWithoutModelImprovement = 1,
  // Editor feature is not allowed.
  kDisallowed = 2,
};
}  // namespace chromeos::editor_menu

#endif  // CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_ENTERPRISE_POLICY_ENUMS_H_

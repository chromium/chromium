// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NONCLOSABLE_APP_UI_NONCLOSABLE_APP_UI_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_NONCLOSABLE_APP_UI_NONCLOSABLE_APP_UI_UTILS_H_

#include <string>

#include "base/component_export.h"

namespace ash {

// This function will show a toast indicating to the user
// that a certain app cannot be closed.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_NONCLOSABLE_APP_UI)
void ShowNonclosableAppToast(const std::string& app_id,
                             const std::string& app_name);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NONCLOSABLE_APP_UI_NONCLOSABLE_APP_UI_UTILS_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_WM_FEATURES_H_
#define CHROMEOS_UI_WM_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace chromeos::wm::features {

COMPONENT_EXPORT(CHROMEOS_UI_WM) BASE_DECLARE_FEATURE(kFloatWindow);

COMPONENT_EXPORT(CHROMEOS_UI_WM) BASE_DECLARE_FEATURE(kPartialSplit);

// Checks if the float feature is enabled. On ash, this checks the feature flag.
// On lacros, this checks the lacros service.
COMPONENT_EXPORT(CHROMEOS_UI_WM) bool IsFloatWindowEnabled();

// Checks if partial split is enabled. On ash, this checks the feature flag.
// False otherwise.
COMPONENT_EXPORT(CHROMEOS_UI_WM) bool IsPartialSplitEnabled();

}  // namespace chromeos::wm::features

#endif  // CHROMEOS_UI_WM_FEATURES_H_

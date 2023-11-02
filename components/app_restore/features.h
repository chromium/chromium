// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_FEATURES_H_
#define COMPONENTS_APP_RESTORE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace full_restore {
namespace features {

// Enables the window state and bounds predictor and full ghost window for ARC++
// apps.
COMPONENT_EXPORT(APP_RESTORE) BASE_DECLARE_FEATURE(kArcWindowPredictor);

// Enables the full restore for Lacros feature. If this is enabled, we will
// restore apps and app windows opened with Lacros after a crash or reboot.
COMPONENT_EXPORT(APP_RESTORE) BASE_DECLARE_FEATURE(kFullRestoreForLacros);

COMPONENT_EXPORT(APP_RESTORE) bool IsArcWindowPredictorEnabled();

COMPONENT_EXPORT(APP_RESTORE) bool IsFullRestoreForLacrosEnabled();

}  // namespace features
}  // namespace full_restore

#endif  // COMPONENTS_APP_RESTORE_FEATURES_H_

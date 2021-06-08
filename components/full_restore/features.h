// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_FEATURES_H_
#define COMPONENTS_FULL_RESTORE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace full_restore {
namespace features {

// Enables the pre-load app window for ARC++ app during ARCVM booting stage on
// full restore process.
COMPONENT_EXPORT(FULL_RESTORE) extern const base::Feature kArcGhostWindow;

// Enables the full restore feature. If this is enabled, we will restore apps
// and app windows after a crash or reboot.
COMPONENT_EXPORT(FULL_RESTORE) extern const base::Feature kFullRestore;

COMPONENT_EXPORT(FULL_RESTORE) bool IsArcGhostWindowEnabled();

COMPONENT_EXPORT(FULL_RESTORE) bool IsFullRestoreEnabled();

}  // namespace features
}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FEATURES_H_

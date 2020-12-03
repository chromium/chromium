// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_
#define COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_

#include "base/component_export.h"

namespace full_restore {

// Returns true if we should restore apps and pages based on the restore setting
// and the user's choice from the notification. Otherwise, returns false.
COMPONENT_EXPORT(FULL_RESTORE) bool ShouldRestore();

// Sets whether we should restore apps and pages, based on the restore setting
// and the user's choice from the notification.
COMPONENT_EXPORT(FULL_RESTORE) void SetRestoreFlag(bool should_restore);

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_

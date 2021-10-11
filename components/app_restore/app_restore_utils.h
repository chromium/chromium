// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_APP_RESTORE_UTILS_H_
#define COMPONENTS_APP_RESTORE_APP_RESTORE_UTILS_H_

#include "base/component_export.h"
#include "ui/views/widget/widget.h"

namespace app_restore {
struct WindowInfo;

// Applies properties from `window_info` to the given `property_handler`.
// This is called from `GetWindowInfo()` when window is
// created, or from the ArcReadHandler when a task is ready for a full
// restore window that has already been created.
COMPONENT_EXPORT(APP_RESTORE)
void ApplyProperties(WindowInfo* window_info,
                     ui::PropertyHandler* property_handler);

// Modifies `out_params` based on the window info associated with
// `restore_window_id`.
COMPONENT_EXPORT(APP_RESTORE)
void ModifyWidgetParams(int32_t restore_window_id,
                        views::Widget::InitParams* out_params);

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_APP_RESTORE_UTILS_H_

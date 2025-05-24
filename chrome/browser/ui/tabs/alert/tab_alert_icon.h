// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERT_TAB_ALERT_ICON_H_
#define CHROME_BROWSER_UI_TABS_ALERT_TAB_ALERT_ICON_H_

#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"

namespace ui {
class ImageModel;
}  // namespace ui

namespace tabs {

enum class TabAlert;

// Returns the corresponding icon for the given alert.
const gfx::VectorIcon& GetAlertIcon(TabAlert alert_state);

// Returns the corresponding image model for the given `alert_state` and
// `icon_color`.
ui::ImageModel GetAlertImageModel(tabs::TabAlert alert_state,
                                  ui::ColorId icon_color);

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_ALERT_TAB_ALERT_ICON_H_

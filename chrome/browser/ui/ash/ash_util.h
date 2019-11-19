// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASH_UTIL_H_

#include <memory>

#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}

namespace ash_util {

// Sets up |params| to place the widget in an ash shell window container on
// the primary display. See ash/public/cpp/shell_window_ids.h for |container_id|
// values.
// TODO(jamescook): Extend to take a display_id.
void SetupWidgetInitParamsForContainer(views::Widget::InitParams* params,
                                       int container_id);

// Returns the the SystemModalContainer id if a session is active, ortherwise
// returns the LockSystemModalContainer id so that the dialog appears above the
// lock screen.
int GetSystemModalDialogContainerId();

// Triggers the window bounce animation.
void BounceWindow(aura::Window* window);

}  // namespace ash_util

#endif  // CHROME_BROWSER_UI_ASH_ASH_UTIL_H_

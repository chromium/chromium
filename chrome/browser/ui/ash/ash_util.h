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

namespace service_manager {
class Connector;
}

namespace ui {
class Accelerator;
class KeyEvent;
}  // namespace ui

namespace ash_util {

// Returns true if the given |accelerator| has been deprecated and hence can
// be consumed by web contents if needed.
bool IsAcceleratorDeprecated(const ui::Accelerator& accelerator);

// Returns true if ash has an accelerator for |key_event| that is enabled.
bool WillAshProcessAcceleratorForEvent(const ui::KeyEvent& key_event);

// Sets up |params| to place the widget in an ash shell window container on
// the primary display. See ash/public/cpp/shell_window_ids.h for |container_id|
// values.
// TODO(jamescook): Extend to take a display_id.
void SetupWidgetInitParamsForContainer(views::Widget::InitParams* params,
                                       int container_id);

// Returns the connector from ServiceManagerConnection::GetForProcess().
// May be null in unit tests.
service_manager::Connector* GetServiceManagerConnector();

// Triggers the window bounce animation inside ash. Handled on the ash side so
// the window frame is included in the bounce and to avoid sending IPCs for
// window transform updates.
void BounceWindow(aura::Window* window);

}  // namespace ash_util

#endif  // CHROME_BROWSER_UI_ASH_ASH_UTIL_H_

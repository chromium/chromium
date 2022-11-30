// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LACROS_WINDOW_PROPERTIES_H_
#define CHROME_BROWSER_UI_LACROS_WINDOW_PROPERTIES_H_

#include "base/component_export.h"
#include "ui/base/class_property.h"

namespace chromeos {
enum class WindowPinType;
}  // namespace chromeos

namespace lacros {

// A property key to store WindowPinType for a window. When setting this
// property to PINNED or TRUSTED_PINNED, the window manager will try to
// fullscreen the window and pin it on the top of the screen. If the window
// manager failed to do it, the property will be restored to NONE. When setting
// this property to NONE, the window manager will restore the window.
COMPONENT_EXPORT(CHROMEOS_LACROS)
extern const ui::ClassProperty<chromeos::WindowPinType>* const
    kWindowPinTypeKey;

}  // namespace lacros

#endif  // CHROME_BROWSER_UI_LACROS_WINDOW_PROPERTIES_H_

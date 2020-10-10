// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_
#define CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_

#include "base/component_export.h"
#include "ui/base/class_property.h"

namespace aura {
template <typename T>
using WindowProperty = ui::ClassProperty<T>;
}  // namespace aura

namespace chromeos {

enum class WindowStateType;

// Shell-specific window property keys for use by ash and lacros clients.

// Alphabetical sort.

// If true, the window is currently showing in overview mode.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const aura::WindowProperty<bool>* const kIsShowingInOverviewKey;

// A property key to indicate ash's extended window state.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const aura::WindowProperty<WindowStateType>* const kWindowStateTypeKey;

// Alphabetical sort.

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_

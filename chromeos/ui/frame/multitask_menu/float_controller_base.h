// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_FLOAT_CONTROLLER_BASE_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_FLOAT_CONTROLLER_BASE_H_

#include "base/component_export.h"

namespace aura {
class Window;
}

namespace chromeos {

// The location of the float window when it first becomes floated. The bottom
// right is the default, unless we are floating from the multitask menu with
// reverse mode active.
enum class FloatStartLocation {
  kBottomRight,
  kBottomLeft,
};

// This interface handles float/unfloat a window.
// The singleton that implements the interface is provided by Ash and Lacros.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) FloatControllerBase {
 public:
  virtual ~FloatControllerBase();

  static FloatControllerBase* Get();

  // Floats and unfloats `window`.
  virtual void SetFloat(aura::Window* window,
                        FloatStartLocation float_start_location) = 0;
  virtual void UnsetFloat(aura::Window* window) = 0;

 protected:
  FloatControllerBase();
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_FLOAT_CONTROLLER_BASE_H_

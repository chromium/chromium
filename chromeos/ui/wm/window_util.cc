// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/window_util.h"

#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"

namespace chromeos {

void ToggleFloating(aura::Window* window) {
  DCHECK(window);
  const bool window_float_state = window->GetProperty(kWindowFloatTypeKey);
  window->SetProperty(kWindowFloatTypeKey, !window_float_state);
}

}  // namespace chromeos

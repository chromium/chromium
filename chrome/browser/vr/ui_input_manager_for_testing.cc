// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_input_manager_for_testing.h"

namespace vr {

UiInputManagerForTesting::UiInputManagerForTesting(UiScene* scene)
    : UiInputManager(scene) {}

bool UiInputManagerForTesting::ControllerRestingInViewport() const {
  return false;
}

}  // namespace vr

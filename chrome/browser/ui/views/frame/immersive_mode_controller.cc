// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

#include "base/observer_list.h"

ImmersiveModeController::ImmersiveModeController() = default;

ImmersiveModeController::~ImmersiveModeController() {
  observers_.Notify(&Observer::OnImmersiveModeControllerDestroyed);
}

void ImmersiveModeController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ImmersiveModeController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

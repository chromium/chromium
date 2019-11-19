// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

ImmersiveModeController::ImmersiveModeController() = default;

ImmersiveModeController::~ImmersiveModeController() {
  for (Observer& observer : observers_)
    observer.OnImmersiveModeControllerDestroyed();
}

void ImmersiveModeController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ImmersiveModeController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

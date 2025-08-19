// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/mock_immersive_mode_controller.h"

MockImmersiveModeController::MockImmersiveModeController() = default;

MockImmersiveModeController::~MockImmersiveModeController() {
  for (auto& observer : observers_) {
    observer.OnImmersiveModeControllerDestroyed();
  }
}

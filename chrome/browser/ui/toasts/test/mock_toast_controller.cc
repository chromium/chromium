// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/test/mock_toast_controller.h"

MockToastController::MockToastController(
    BrowserWindowInterface* browser_window_interface,
    const ToastRegistry* toast_registry)
    : ToastController(browser_window_interface, toast_registry) {}

MockToastController::~MockToastController() = default;

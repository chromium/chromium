// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_controller.h"

#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"

ToastParams::ToastParams(ToastId id) : toast_id_(id) {}
ToastParams::~ToastParams() = default;

ToastController::ToastController(BrowserView* browser_view)
    : browser_view_(browser_view) {}
ToastController::~ToastController() = default;

bool ToastController::CanShowToast(ToastId id) {
  // TODO(crbug.com/358609791): Implement controller to work with the toast
  // service.
  return false;
}

void ToastController::ShowToast(ToastParams params) {
  // TODO(crbug.com/358609791): Implement controller to work with the toast
  // service.
}

void ToastController::ClosePersistentToast(ToastId id) {
  // TODO(crbug.com/358609791): Implement controller to work with the toast
  // service.
}

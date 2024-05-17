// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/screen_capture_permission_checker_mac.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/cocoa/permissions_utils.h"
#include "ui/base/ui_base_features.h"

BASE_FEATURE(kDesktopCapturePermissionChecker,
             "DesktopCapturePermissionChecker",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kDesktopCapturePermissionCheckerUpdateIntervalMs{
    &kDesktopCapturePermissionChecker, "update_interval_ms", 1000};

std::unique_ptr<ScreenCapturePermissionCheckerMac>
ScreenCapturePermissionCheckerMac::MaybeCreate(
    base::RepeatingCallback<void(bool)> callback) {
  if (base::FeatureList::IsEnabled(kDesktopCapturePermissionChecker)) {
    return std::make_unique<ScreenCapturePermissionCheckerMac>(callback);
  }
  return nullptr;
}

ScreenCapturePermissionCheckerMac::ScreenCapturePermissionCheckerMac(
    base::RepeatingCallback<void(bool)> callback)
    : callback_(callback) {
  OnRecurrentPermissionCheck();
  // Passing `this` to `timer_.Start` is safe since `timer_` is owned by `this`.
  timer_.Start(FROM_HERE,
               base::Milliseconds(
                   kDesktopCapturePermissionCheckerUpdateIntervalMs.Get()),
               this,
               &ScreenCapturePermissionCheckerMac::OnRecurrentPermissionCheck);
}

ScreenCapturePermissionCheckerMac::~ScreenCapturePermissionCheckerMac() =
    default;

void ScreenCapturePermissionCheckerMac::Stop() {
  timer_.Stop();
}

void ScreenCapturePermissionCheckerMac::OnRecurrentPermissionCheck() {
  if (has_pending_task_) {
    return;
  }
  has_pending_task_ = true;

  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ui::IsScreenCaptureAllowed),
      base::BindOnce(&ScreenCapturePermissionCheckerMac::OnPermissionUpdate,
                     weak_factory_.GetWeakPtr()));
}

void ScreenCapturePermissionCheckerMac::OnPermissionUpdate(
    bool has_permission) {
  has_pending_task_ = false;

  callback_.Run(has_permission);
}

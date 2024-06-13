// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/screen_capture_permission_checker_mac.h"

#include "base/metrics/histogram_functions.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/cocoa/permissions_utils.h"
#include "ui/base/ui_base_features.h"

// If enabled, a `ScreenCapturePermissionChecker` will be instantiated.
// * The `ScreenCapturePermissionChecker` object *will* record the initial
//   screen-sharing permission status.
// * Depending on the state of the `kDesktopCapturePermissionChecker` feature
//   defined below, the `ScreenCapturePermissionChecker` object may additionally
//   send updates on the system's screen-recording permission-state, which may
//   be used to display a permission notification and a button to open the
//   screen-recording settings when the permission is missing.
BASE_FEATURE(kDesktopCapturePermissionCheckerKillSwitch,
             "DesktopCapturePermissionCheckerKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Only has an effect if `kDesktopCapturePermissionCheckerKillSwitch` is
// enabled, in which case:
// * If the user is missing screen-sharing permissions, then when
//   `kDesktopCapturePermissionChecker` is enabled, users will be presented
//   with a button letting them quickly jump into the OS settings where they
//   can grant the missing permission.
// * If the user has screen-sharing permission, `kScreenCapturePermissionButton`
//   will not have any user-visible effect.
BASE_FEATURE(kDesktopCapturePermissionChecker,
             "DesktopCapturePermissionChecker",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kDesktopCapturePermissionCheckerUpdateIntervalMs{
    &kDesktopCapturePermissionChecker, "update_interval_ms", 1000};

std::unique_ptr<ScreenCapturePermissionCheckerMac>
ScreenCapturePermissionCheckerMac::MaybeCreate(
    base::RepeatingCallback<void(bool)> callback) {
  if (base::FeatureList::IsEnabled(
          kDesktopCapturePermissionCheckerKillSwitch)) {
    return std::make_unique<ScreenCapturePermissionCheckerMac>(callback);
  }
  return nullptr;
}

ScreenCapturePermissionCheckerMac::ScreenCapturePermissionCheckerMac(
    base::RepeatingCallback<void(bool)> callback)
    : callback_(callback) {
  OnRecurrentPermissionCheck();

  if (!base::FeatureList::IsEnabled(kDesktopCapturePermissionChecker)) {
    return;
  }

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

  if (!has_recorded_uma_) {
    base::UmaHistogramBoolean(
        "Media.Ui.GetDisplayMedia.HasScreenRecordingPermission",
        has_permission);
    has_recorded_uma_ = true;
  }

  if (!base::FeatureList::IsEnabled(kDesktopCapturePermissionChecker)) {
    return;
  }

  callback_.Run(has_permission);
}

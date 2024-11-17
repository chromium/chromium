// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/screen_capture_permission_checker_mac.h"

#include "base/mac/mac_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/cocoa/permissions_utils.h"
#include "ui/base/ui_base_features.h"

// If enabled, a `ScreenCapturePermissionChecker` will be instantiated.
// * The `ScreenCapturePermissionChecker` object *will* record the initial
//   screen-sharing permission status.
// * Depending on the state of the `kDesktopCapturePermissionChecker*` features
//   defined below, the `ScreenCapturePermissionChecker` object may additionally
//   send updates on the system's screen-recording permission-state, which may
//   be used to display a permission notification and a button to open the
//   screen-recording settings when the permission is missing.
BASE_FEATURE(kDesktopCapturePermissionCheckerKillSwitch,
             "DesktopCapturePermissionCheckerKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Only has an effect if `kDesktopCapturePermissionCheckerKillSwitch` is enabled
// and macOS 14.4+ is used, in which case:
// * If the user is missing screen-sharing permissions, then when
//   `kDesktopCapturePermissionCheckerMacos14_4Plus` is enabled, users will be
//   presented with a button letting them quickly jump into the OS settings
//   where they can grant the missing permission.
// * If the user has screen-sharing permission,
// `kDesktopCapturePermissionCheckerMacos14_4Plus` will not have any
// user-visible effect.
BASE_FEATURE(kDesktopCapturePermissionCheckerMacos14_4Plus,
             "DesktopCapturePermissionCheckerMacos14_4Plus",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature is the same as `kDesktopCapturePermissionCheckerMacos14_4Plus`
// but only affects macOS <14.4.
BASE_FEATURE(kDesktopCapturePermissionCheckerPreMacos14_4,
             "DesktopCapturePermissionCheckerPreMacos14_4",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kDesktopCapturePermissionCheckerUpdateIntervalMs{
    &kDesktopCapturePermissionCheckerMacos14_4Plus, "update_interval_ms", 1000};

namespace {
bool IsDesktopCapturePermissionCheckerEnabled() {
  const int macos_version = base::mac::MacOSVersion();
  if (macos_version >= 14'04'00 &&
      !base::FeatureList::IsEnabled(
          kDesktopCapturePermissionCheckerMacos14_4Plus)) {
    return false;
  }
  if (macos_version < 14'04'00 &&
      !base::FeatureList::IsEnabled(
          kDesktopCapturePermissionCheckerPreMacos14_4)) {
    return false;
  }
  return true;
}
}  // namespace

std::unique_ptr<ScreenCapturePermissionCheckerMac>
ScreenCapturePermissionCheckerMac::MaybeCreate(
    base::RepeatingCallback<void(bool)> callback) {
  if (!system_media_permissions::ScreenCaptureNeedsSystemLevelPermissions()) {
    return nullptr;
  }

  if (base::FeatureList::IsEnabled(
          kDesktopCapturePermissionCheckerKillSwitch) &&
      base::mac::MacOSMajorVersion() >= 13) {
    return std::make_unique<ScreenCapturePermissionCheckerMac>(
        callback, base::BindRepeating(&ui::IsScreenCaptureAllowed));
  }
  return nullptr;
}

ScreenCapturePermissionCheckerMac::ScreenCapturePermissionCheckerMac(
    base::RepeatingCallback<void(bool)> callback,
    base::RepeatingCallback<bool()> is_screen_capture_allowed)
    : callback_(std::move(callback)),
      is_screen_capture_allowed_(std::move(is_screen_capture_allowed)) {
  OnRecurrentPermissionCheck();

  if (!IsDesktopCapturePermissionCheckerEnabled()) {
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
      FROM_HERE, is_screen_capture_allowed_,
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

  if (!IsDesktopCapturePermissionCheckerEnabled()) {
    return;
  }

  callback_.Run(has_permission);
}

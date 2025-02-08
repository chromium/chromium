// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/screen_capture_permission_checker.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/desktop_capture/screen_capture_permission_checker_mac.h"
#endif

std::unique_ptr<ScreenCapturePermissionChecker>
ScreenCapturePermissionChecker::MaybeCreate(
    base::RepeatingCallback<void(bool)> callback) {
#if BUILDFLAG(IS_MAC)
  return ScreenCapturePermissionCheckerMac::MaybeCreate(callback);
#else
  return nullptr;
#endif
}

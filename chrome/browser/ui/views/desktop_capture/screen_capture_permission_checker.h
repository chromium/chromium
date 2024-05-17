// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SCREEN_CAPTURE_PERMISSION_CHECKER_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SCREEN_CAPTURE_PERMISSION_CHECKER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

class ScreenCapturePermissionChecker {
 public:
  // Create a ScreenCapturePermissionChecker if there is one available and
  // enabled for this platform.
  static std::unique_ptr<ScreenCapturePermissionChecker> MaybeCreate(
      base::RepeatingCallback<void(bool)> callback);

  virtual ~ScreenCapturePermissionChecker() = default;

  virtual void Stop() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SCREEN_CAPTURE_PERMISSION_CHECKER_H_

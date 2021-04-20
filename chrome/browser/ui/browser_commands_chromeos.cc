// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands_chromeos.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"

using base::UserMetricsAction;

void TakeScreenshot() {
  base::RecordAction(UserMetricsAction("Menu_Take_Screenshot"));
  ChromeScreenshotGrabber* grabber = ChromeScreenshotGrabber::Get();
  if (grabber->CanTakeScreenshot())
    grabber->HandleTakeScreenshotForAllRootWindows();
}

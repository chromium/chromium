// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands_chromeos.h"

#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

using base::UserMetricsAction;

void TakeScreenshot() {
  base::RecordAction(UserMetricsAction("Menu_Take_Screenshot"));
  ash::CaptureScreenshotsOfAllDisplays();
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands_chromeos.h"

#include "ash/public/cpp/desks_helper.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;

void SendToDeskAtIndex(Browser* browser, int desk_index) {
  ash::DesksHelper::Get()->SendToDeskAtIndex(
      browser->window()->GetNativeWindow(), desk_index);
}

void TakeScreenshot() {
  base::RecordAction(UserMetricsAction("Menu_Take_Screenshot"));
  ChromeScreenshotGrabber* grabber = ChromeScreenshotGrabber::Get();
  if (grabber->CanTakeScreenshot())
    grabber->HandleTakeScreenshotForAllRootWindows();
}

void ToggleAssignedToAllDesks(Browser* browser) {
  auto* widget = views::Widget::GetWidgetForNativeWindow(
      browser->window()->GetNativeWindow());
  DCHECK(widget);
  widget->SetVisibleOnAllWorkspaces(!widget->IsVisibleOnAllWorkspaces());
}

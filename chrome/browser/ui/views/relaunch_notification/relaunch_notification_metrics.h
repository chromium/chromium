// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_METRICS_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_METRICS_H_

namespace relaunch_notification {

// Record to UMA showing a relaunch recommended notification.
void RecordRecommendedShowResult();

// Record to UMA showing a relaunch required notification.
void RecordRequiredShowResult();

}  // namespace relaunch_notification

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_METRICS_H_

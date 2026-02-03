// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_ENGAGEMENT_CONTENT_ENGAGEMENT_TYPE_H_
#define COMPONENTS_SITE_ENGAGEMENT_CONTENT_ENGAGEMENT_TYPE_H_

namespace site_engagement {

// This is used to back a UMA histogram, so it should be treated as
// append-only. Any new values should be inserted immediately prior to
// kLast and added to SiteEngagementServiceEngagementType in
// tools/metrics/histograms/enums.xml.
// TODO(calamity): Document each of these engagement types.
enum class EngagementType {
  kNavigation = 0,
  kKeypress = 1,
  kMouse = 2,
  kTouchGesture = 3,
  kScroll = 4,
  kMediaHidden = 5,
  kMediaVisible = 6,
  kWebappShortcutLaunch = 7,
  kFirstDailyEngagement = 8,
  kNotificationInteraction = 9,
  kLast,
};

}  // namespace site_engagement

#endif  // COMPONENTS_SITE_ENGAGEMENT_CONTENT_ENGAGEMENT_TYPE_H_

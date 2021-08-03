// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTIVE_PORTAL_CORE_CAPTIVE_PORTAL_METRICS_H_
#define COMPONENTS_CAPTIVE_PORTAL_CORE_CAPTIVE_PORTAL_METRICS_H_

#include "base/macros.h"
#include "components/captive_portal/core/captive_portal_export.h"

namespace captive_portal {

// Class which defines metrics used for tracking user interaction with captive
// portals.
class CAPTIVE_PORTAL_EXPORT CaptivePortalMetrics {
 public:
  // User action when the user is shown a captive portal error page.
  enum CaptivePortalBlockingPageEvent {
    SHOW_ALL,
    OPEN_LOGIN_PAGE,
    CAPTIVE_PORTAL_BLOCKING_PAGE_EVENT_COUNT
  };

  // Logs a user action when the user is shown a captive portal error page.
  static void LogCaptivePortalBlockingPageEvent(
      CaptivePortalBlockingPageEvent event);

  DISALLOW_IMPLICIT_CONSTRUCTORS(CaptivePortalMetrics);
};

}  // namespace captive_portal

#endif  // COMPONENTS_CAPTIVE_PORTAL_CORE_CAPTIVE_PORTAL_METRICS_H_

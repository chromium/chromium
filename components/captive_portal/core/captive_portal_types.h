// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTIVE_PORTAL_CORE_CAPTIVE_PORTAL_TYPES_H_
#define COMPONENTS_CAPTIVE_PORTAL_CORE_CAPTIVE_PORTAL_TYPES_H_

#include <string>

#include "components/captive_portal/core/captive_portal_export.h"

namespace captive_portal {

// Possible results of an attempt to detect a captive portal.
enum CaptivePortalResult {
  // There's a confirmed connection to the Internet.
  RESULT_INTERNET_CONNECTED,
  // The URL request received a network or HTTP error, or a non-HTTP response.
  RESULT_NO_RESPONSE,
  // The URL request apparently encountered a captive portal.  It received a
  // a valid HTTP response with a 2xx other than 204, 3xx, or 511 status code.
  RESULT_BEHIND_CAPTIVE_PORTAL,
  RESULT_COUNT
};

// Captive portal type of a given WebContents.
enum class CaptivePortalWindowType {
  // This browser is not used for captive portal resolution, i.e. it's not a
  // captive portal login window.
  kNone,
  // This browser is a popup for captive portal resolution.
  kPopup,
  // This browser is a tab for captive portal resolution.
  kTab,
};

CAPTIVE_PORTAL_EXPORT extern std::string CaptivePortalResultToString(
    CaptivePortalResult result);

}  // namespace captive_portal

#endif  // COMPONENTS_CAPTIVE_PORTAL_CORE_CAPTIVE_PORTAL_TYPES_H_

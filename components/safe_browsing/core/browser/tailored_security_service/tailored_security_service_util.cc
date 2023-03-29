// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_util.h"

#include "base/metrics/histogram_functions.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_notification_result.h"

namespace safe_browsing {

void RecordEnabledNotificationResult(
    TailoredSecurityNotificationResult result) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.TailoredSecurity.SyncPromptEnabledNotificationResult2",
      result);
}

}  // namespace safe_browsing
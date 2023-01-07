// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_UTIL_H_

enum class TailoredSecurityNotificationResult;

namespace safe_browsing {

// Records an UMA Histogram value to count the result of trying to notify a sync
// user about enhanced protection for the enable case.
void RecordEnabledNotificationResult(TailoredSecurityNotificationResult result);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_UTIL_H_

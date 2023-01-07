// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_RESOURCE_REQUEST_BLOCKED_REASON_H_
#define CHROME_COMMON_CHROME_RESOURCE_REQUEST_BLOCKED_REASON_H_

#include "content/public/common/resource_request_blocked_reason.h"

// Extends content::ResourceRequestBlockedReason with Chrome specific reasons.
enum class ChromeResourceRequestBlockedReason {
  kExtension =
      static_cast<int>(content::ResourceRequestBlockedReason::kMax) + 1,
  kMax = kExtension,
};

#endif  // CHROME_COMMON_CHROME_RESOURCE_REQUEST_BLOCKED_REASON_H_

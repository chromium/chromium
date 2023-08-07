// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/safebrowsing_constants.h"

#include "components/safe_browsing/core/common/features.h"
#include "net/base/net_errors.h"

namespace safe_browsing {

const base::FilePath::CharType kSafeBrowsingBaseFilename[] =
    FILE_PATH_LITERAL("Safe Browsing");

const base::FilePath::CharType kCookiesFile[] = FILE_PATH_LITERAL(" Cookies");

const char kCustomCancelReasonForURLLoader[] = "SafeBrowsing";

const int kNetErrorCodeForSafeBrowsing = net::ERR_BLOCKED_BY_CLIENT;

const char kSafeBrowsingEnabledHistogramName[] = "SafeBrowsing.Pref.General";

const char kArtificialCachedPhishGuardVerdictFlag[] =
    "mark_as_phish_guard_phishing";

const char kArtificialCachedHashPrefixRealTimeVerdictFlag[] =
    "mark_as_hash_prefix_real_time_phishing";

const std::vector<std::string> GetExcludedCountries() {
  // Safe Browsing endpoint doesn't exist.
  return {"cn"};
}

const char kFlaggedUrl[] = "flagged_url";

const char kMainFrameUrl[] = "main_frame_url";

const char kReferrerUrl[] = "referrer_url";

const char kUserActivityWithUrls[] = "user_activity_with_urls";
}  // namespace safe_browsing

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/common/safebrowsing_constants.h"

#include "components/safe_browsing/features.h"
#include "net/base/net_errors.h"

namespace safe_browsing {

const base::FilePath::CharType kSafeBrowsingBaseFilename[] =
    FILE_PATH_LITERAL("Safe Browsing");

const base::FilePath::CharType kCookiesFile[] = FILE_PATH_LITERAL(" Cookies");

// The URL for the Safe Browsing page.
const char kSafeBrowsingUrl[] = "https://safebrowsing.google.com/";

const char kCustomCancelReasonForURLLoader[] = "SafeBrowsing";

int GetNetErrorCodeForSafeBrowsing() {
  return base::FeatureList::IsEnabled(safe_browsing::kCommittedSBInterstitials)
             ? net::ERR_BLOCKED_BY_CLIENT
             : net::ERR_ABORTED;
}

}  // namespace safe_browsing

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

const std::vector<std::string> GetExcludedCountries() {
  // Safe Browsing endpoint doesn't exist.
  return {"cn"};
}

const char kFlaggedUrl[] = "Flagged URL";

const char kMainFrameUrl[] = "Main frame URL";

const char kReferrerUrl[] = "Referrer URL";

const char kReferringApp[] = "Referring app";

const char kTimeWarningVisible[] = "Time warning visible";

const char kUserActivityWithUrls[] = "User activity with URLs";

const char kUserAction[] = "User action";

const char kLearnMoreClicked[] = "Learn more clicked";

const char kShowMoreClicked[] = "Show more clicked";

const char kOpenDiagnostic[] = "Open diagnostic clicked";

const char kRepeatVisit[] = "Repeat visit";

const char kReportPhishingErrorClicked[] = "Report phishing error clicked";

const char kReportType[] = "Report type";

const char kUserActionUnknown[] = "UNKNOWN";

const char kUserActionProceed[] = "PROCEED";

const char kUserActionDontProceed[] = "DONT_PROCEED";

const char kUserActionCloseTab[] = "CLOSE_TAB";

const char kUserActionNavigateAway[] = "NAVIGATE_AWAY";

}  // namespace safe_browsing

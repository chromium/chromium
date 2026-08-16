// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_CONSTANTS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_CONSTANTS_H_

#include <string>

#include "base/files/file_path.h"

namespace safe_browsing {

extern const base::FilePath::CharType kSafeBrowsingBaseFilename[];

// Filename suffix for the cookie database.
extern const base::FilePath::CharType kCookiesFile[];

// When a network::mojom::URLLoader is cancelled because of SafeBrowsing, this
// custom cancellation reason could be used to notify the implementation side.
// Please see network::mojom::URLLoader::kClientDisconnectReason for more
// details.
extern const char kCustomCancelReasonForURLLoader[];

// error_code to use when Safe Browsing blocks a request.
extern const int kNetErrorCodeForSafeBrowsing;

// The name of the histogram that records whether Safe Browsing is enabled.
extern const char kSafeBrowsingEnabledHistogramName[];

// Countries that has no endpoint for Safe Browsing.
const std::vector<std::string> GetExcludedCountries();

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CredentialPhishedStatus {
  // The credential was just marked as phished.
  kMarkedAsPhished = 0,
  // The credential's site was marked as legitimate.
  kSiteMarkedAsLegitimate = 1,
  kMaxValue = kSiteMarkedAsLegitimate,
};

// Product Specific (String) Data field name for the URL that triggered the
// warning.
extern const char kFlaggedUrl[];

// Product Specific (String) Data field name for the main frame URL.
extern const char kMainFrameUrl[];

// Product Specific (String) Data field name for the referrer URL.
extern const char kReferrerUrl[];

// Product Specific (String) Data field name for the referring app.
extern const char kReferringApp[];

// Product Specific (String) Data field name for the time the warning was
// visible.
extern const char kTimeWarningVisible[];

// Product Specific (String) Data field name for user activity.
extern const char kUserActivityWithUrls[];

// Product Specific (String) Data field name for user action.
extern const char kUserAction[];

// Product Specific (Bits) Data field name for whether the user clicked learn
// more.
extern const char kLearnMoreClicked[];

// Product Specific (Bits) Data field name for whether the user clicked show
// more on the interstitial.
extern const char kShowMoreClicked[];

// Product Specific (Bits) Data field name for whether the user clicked the
// diagnostic link on the interstitial.
extern const char kOpenDiagnostic[];

// Product Specific (Bits) Data field name for whether the user visited this
// site before.
extern const char kRepeatVisit[];

// Product Specific (Bits) Data field name for whether the user clicked report
// phishing error link on the interstitial.
extern const char kReportPhishingErrorClicked[];

// Product Specific (String) Data field name for the threat report type.
extern const char kReportType[];

// Product Specific Data value for user action when the action could not be
// determined (e.g., when the threat report contains no interaction events or
// resolution status).
extern const char kUserActionUnknown[];

// Product Specific Data value for user action when the user proceeded past the
// warning.
extern const char kUserActionProceed[];

// Product Specific Data value for user action when the user decided not to
// proceed past the warning.
extern const char kUserActionDontProceed[];

// Product Specific Data value for user action when the user closed the tab.
extern const char kUserActionCloseTab[];

// Product Specific Data value for user action when the user navigated away
// without interacting with the UI.
extern const char kUserActionNavigateAway[];

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_CONSTANTS_H_

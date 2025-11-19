// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CREDIT_CARD_FORM_EVENT_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CREDIT_CARD_FORM_EVENT_H_

#include <optional>
#include <string>
#include <string_view>

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/safe_browsing/core/browser/referring_app_info.h"  // nogncheck
#endif

namespace safe_browsing::credit_card_form {

enum SiteVisit {
  kUnknownSiteVisit = 0,
  kNewSiteVisit = 1,
  kRepeatSiteVisit = 2,
  kSiteVisitMaxValue = kRepeatSiteVisit,
};

enum ReferringApp {
  kNoReferringApp = 0,
  kOtherApp = 1,
  kChrome = 2,
  kSmsApp = 3,
  kReferringAppMaxValue = kSmsApp,
};

enum FieldDetectionHeuristic {
  kNoDetectionHeuristic = 0,
  kAutofillLocal = 1,
  kAutofillServer = 2,
  kFieldDetectionHeuristicMaxValue = kAutofillServer,
};

// An enum representing all permutations of details pertaining to a credit
// card form event that may trigger a CSD ping:
//   * user site visit history
//   * referring app (Android only)
//   * form field detection heuristics
//
// Enum values are sparse and determined by considering each component as
// a distinct order of magnitude. For example, if the three component enum
// values are 3, 1, and 2, then the corresponding enum value here is 312.
// None of these is expected to end up with more than 10 values.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum CreditCardFormEvent {
  kCreditCardFormEventMinValue = 0,

  kUnknownSiteVisitNoReferringAppNoDetectionHeuristic = 0,
  kUnknownSiteVisitNoReferringAppAutofillLocalHeuristic = 1,
  kUnknownSiteVisitNoReferringAppAutofillServerHeuristic = 2,

  kUnknownSiteVisitOtherReferringAppNoDetectionHeuristic = 10,
  kUnknownSiteVisitOtherReferringAppAutofillLocalHeuristic = 11,
  kUnknownSiteVisitOtherReferringAppAutofillServerHeuristic = 12,

  kUnknownSiteVisitChromeReferringAppNoDetectionHeuristic = 20,
  kUnknownSiteVisitChromeReferringAppAutofillLocalHeuristic = 21,
  kUnknownSiteVisitChromeReferringAppAutofillServerHeuristic = 22,

  kUnknownSiteVisitSmsReferringAppNoDetectionHeuristic = 30,
  kUnknownSiteVisitSmsReferringAppAutofillLocalHeuristic = 31,
  kUnknownSiteVisitSmsReferringAppAutofillServerHeuristic = 32,

  kNewSiteVisitNoReferringAppNoDetectionHeuristic = 100,
  kNewSiteVisitNoReferringAppAutofillLocalHeuristic = 101,
  kNewSiteVisitNoReferringAppAutofillServerHeuristic = 102,

  kNewSiteVisitOtherReferringAppNoDetectionHeuristic = 110,
  kNewSiteVisitOtherReferringAppAutofillLocalHeuristic = 111,
  kNewSiteVisitOtherReferringAppAutofillServerHeuristic = 112,

  kNewSiteVisitChromeReferringAppNoDetectionHeuristic = 120,
  kNewSiteVisitChromeReferringAppAutofillLocalHeuristic = 121,
  kNewSiteVisitChromeReferringAppAutofillServerHeuristic = 122,

  kNewSiteVisitSmsReferringAppNoDetectionHeuristic = 130,
  kNewSiteVisitSmsReferringAppAutofillLocalHeuristic = 131,
  kNewSiteVisitSmsReferringAppAutofillServerHeuristic = 132,

  kRepeatSiteVisitNoReferringAppNoDetectionHeuristic = 200,
  kRepeatSiteVisitNoReferringAppAutofillLocalHeuristic = 201,
  kRepeatSiteVisitNoReferringAppAutofillServerHeuristic = 202,

  kRepeatSiteVisitOtherReferringAppNoDetectionHeuristic = 210,
  kRepeatSiteVisitOtherReferringAppAutofillLocalHeuristic = 211,
  kRepeatSiteVisitOtherReferringAppAutofillServerHeuristic = 212,

  kRepeatSiteVisitChromeReferringAppNoDetectionHeuristic = 220,
  kRepeatSiteVisitChromeReferringAppAutofillLocalHeuristic = 221,
  kRepeatSiteVisitChromeReferringAppAutofillServerHeuristic = 222,

  kRepeatSiteVisitSmsReferringAppNoDetectionHeuristic = 230,
  kRepeatSiteVisitSmsReferringAppAutofillLocalHeuristic = 231,
  kRepeatSiteVisitSmsReferringAppAutofillServerHeuristic = 232,

  kCreditCardFormEventMaxValue =
      kRepeatSiteVisitSmsReferringAppAutofillServerHeuristic,
};

CreditCardFormEvent GetCreditCardFormEvent(SiteVisit site_visit,
                                           ReferringApp referring_app,
                                           FieldDetectionHeuristic heuristic);

#if BUILDFLAG(IS_ANDROID)

// Translates a ReferringAppInfo to the matching ReferringApp value.
ReferringApp FromReferringAppInfo(internal::ReferringAppInfo info);

void LogEvent(std::string_view event_name,
              SiteVisit site_visit,
              ReferringApp referring_app,
              FieldDetectionHeuristic field_heuristic);

#endif

void LogEvent(std::string_view event_name,
              SiteVisit site_visit,
              FieldDetectionHeuristic field_heuristic);

}  // namespace safe_browsing::credit_card_form

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CREDIT_CARD_FORM_EVENT_H_

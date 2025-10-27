// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CREDIT_CARD_FORM_EVENT_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CREDIT_CARD_FORM_EVENT_H_

#include <optional>
#include <string>
#include <string_view>

namespace safe_browsing::credit_card_form {

enum SiteVisit {
  kUnknownSiteVisit = 0,
  kNewSiteVisit = 1,
  kRepeatSiteVisit = 2,
  kSiteVisitMaxValue = kRepeatSiteVisit,
};

enum ReferringApp {
  kNoReferringApp = 0,
  kChrome = 1,
  kSmsApp = 2,
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

  kUnknownSiteVisitChromeReferringAppNoDetectionHeuristic = 10,
  kUnknownSiteVisitChromeReferringAppAutofillLocalHeuristic = 11,
  kUnknownSiteVisitChromeReferringAppAutofillServerHeuristic = 12,

  kUnknownSiteVisitSmsReferringAppNoDetectionHeuristic = 20,
  kUnknownSiteVisitSmsReferringAppAutofillLocalHeuristic = 21,
  kUnknownSiteVisitSmsReferringAppAutofillServerHeuristic = 22,

  kNewSiteVisitNoReferringAppNoDetectionHeuristic = 100,
  kNewSiteVisitNoReferringAppAutofillLocalHeuristic = 101,
  kNewSiteVisitNoReferringAppAutofillServerHeuristic = 102,

  kNewSiteVisitChromeReferringAppNoDetectionHeuristic = 110,
  kNewSiteVisitChromeReferringAppAutofillLocalHeuristic = 111,
  kNewSiteVisitChromeReferringAppAutofillServerHeuristic = 112,

  kNewSiteVisitSmsReferringAppNoDetectionHeuristic = 120,
  kNewSiteVisitSmsReferringAppAutofillLocalHeuristic = 121,
  kNewSiteVisitSmsReferringAppAutofillServerHeuristic = 122,

  kRepeatSiteVisitNoReferringAppNoDetectionHeuristic = 200,
  kRepeatSiteVisitNoReferringAppAutofillLocalHeuristic = 201,
  kRepeatSiteVisitNoReferringAppAutofillServerHeuristic = 202,

  kRepeatSiteVisitChromeReferringAppNoDetectionHeuristic = 210,
  kRepeatSiteVisitChromeReferringAppAutofillLocalHeuristic = 211,
  kRepeatSiteVisitChromeReferringAppAutofillServerHeuristic = 212,

  kRepeatSiteVisitSmsReferringAppNoDetectionHeuristic = 220,
  kRepeatSiteVisitSmsReferringAppAutofillLocalHeuristic = 221,
  kRepeatSiteVisitSmsReferringAppAutofillServerHeuristic = 222,

  kCreditCardFormEventMaxValue =
      kRepeatSiteVisitSmsReferringAppAutofillServerHeuristic,
};

CreditCardFormEvent GetCreditCardFormEvent(SiteVisit site_visit,
                                           ReferringApp referring_app,
                                           FieldDetectionHeuristic heuristic);

// TODO: crbug.com/443098659 - Add parameters to determine the
// appropriate CreditCardFormEvent permutation to use.
void LogEvent(std::string_view event_name, SiteVisit site_visit);

std::string ToString(SiteVisit site_visit);
std::string ToString(ReferringApp referring_app);
std::string ToString(FieldDetectionHeuristic heuristic);

}  // namespace safe_browsing::credit_card_form

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CREDIT_CARD_FORM_EVENT_H_

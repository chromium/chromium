// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/credit_card_form_event.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing::credit_card_form {

std::string ToString(SiteVisit site_visit) {
  switch (site_visit) {
    case kUnknownSiteVisit:
      return "UnknownSiteVisit";
    case kNewSiteVisit:
      return "NewSiteVisit";
    case kRepeatSiteVisit:
      return "RepeatSiteVisit";
  }
}

std::string ToString(ReferringApp referring_app) {
  switch (referring_app) {
    case kNoReferringApp:
      return "NoReferringApp";
    case kChrome:
      return "Chrome";
    case kSmsApp:
      return "SmsApp";
  }
}

std::string ToString(FieldDetectionHeuristic heuristic) {
  switch (heuristic) {
    case kNoDetectionHeuristic:
      return "NoDetectionHeuristic";
    case kAutofillLocal:
      return "AutofillLocal";
    case kAutofillServer:
      return "AutofillServer";
  }
}

struct GetCreditCardFormEventTestCase {
  SiteVisit site_visit;
  ReferringApp referring_app;
  FieldDetectionHeuristic heuristic;
  CreditCardFormEvent expected_event;

  static std::string GetTestName(
      const testing::TestParamInfo<GetCreditCardFormEventTestCase>& test_case) {
    return base::StrCat({
        ToString(test_case.param.site_visit),
        "_",
        ToString(test_case.param.referring_app),
        "_",
        ToString(test_case.param.heuristic),
    });
  }
};

class GetCreditCardFormEventTest
    : public testing::TestWithParam<GetCreditCardFormEventTestCase> {};

constexpr GetCreditCardFormEventTestCase
    get_credit_card_form_event_test_cases[] = {
        {kUnknownSiteVisit, kNoReferringApp, kNoDetectionHeuristic,
         kUnknownSiteVisitNoReferringAppNoDetectionHeuristic},
        {kUnknownSiteVisit, kNoReferringApp, kAutofillLocal,
         kUnknownSiteVisitNoReferringAppAutofillLocalHeuristic},
        {kUnknownSiteVisit, kNoReferringApp, kAutofillServer,
         kUnknownSiteVisitNoReferringAppAutofillServerHeuristic},
        {kUnknownSiteVisit, kChrome, kNoDetectionHeuristic,
         kUnknownSiteVisitChromeReferringAppNoDetectionHeuristic},
        {kUnknownSiteVisit, kChrome, kAutofillLocal,
         kUnknownSiteVisitChromeReferringAppAutofillLocalHeuristic},
        {kUnknownSiteVisit, kChrome, kAutofillServer,
         kUnknownSiteVisitChromeReferringAppAutofillServerHeuristic},
        {kUnknownSiteVisit, kSmsApp, kNoDetectionHeuristic,
         kUnknownSiteVisitSmsReferringAppNoDetectionHeuristic},
        {kUnknownSiteVisit, kSmsApp, kAutofillLocal,
         kUnknownSiteVisitSmsReferringAppAutofillLocalHeuristic},
        {kUnknownSiteVisit, kSmsApp, kAutofillServer,
         kUnknownSiteVisitSmsReferringAppAutofillServerHeuristic},
        {kNewSiteVisit, kNoReferringApp, kNoDetectionHeuristic,
         kNewSiteVisitNoReferringAppNoDetectionHeuristic},
        {kNewSiteVisit, kNoReferringApp, kAutofillLocal,
         kNewSiteVisitNoReferringAppAutofillLocalHeuristic},
        {kNewSiteVisit, kNoReferringApp, kAutofillServer,
         kNewSiteVisitNoReferringAppAutofillServerHeuristic},
        {kNewSiteVisit, kChrome, kNoDetectionHeuristic,
         kNewSiteVisitChromeReferringAppNoDetectionHeuristic},
        {kNewSiteVisit, kChrome, kAutofillLocal,
         kNewSiteVisitChromeReferringAppAutofillLocalHeuristic},
        {kNewSiteVisit, kChrome, kAutofillServer,
         kNewSiteVisitChromeReferringAppAutofillServerHeuristic},
        {kNewSiteVisit, kSmsApp, kNoDetectionHeuristic,
         kNewSiteVisitSmsReferringAppNoDetectionHeuristic},
        {kNewSiteVisit, kSmsApp, kAutofillLocal,
         kNewSiteVisitSmsReferringAppAutofillLocalHeuristic},
        {kNewSiteVisit, kSmsApp, kAutofillServer,
         kNewSiteVisitSmsReferringAppAutofillServerHeuristic},
        {kRepeatSiteVisit, kNoReferringApp, kNoDetectionHeuristic,
         kRepeatSiteVisitNoReferringAppNoDetectionHeuristic},
        {kRepeatSiteVisit, kNoReferringApp, kAutofillLocal,
         kRepeatSiteVisitNoReferringAppAutofillLocalHeuristic},
        {kRepeatSiteVisit, kNoReferringApp, kAutofillServer,
         kRepeatSiteVisitNoReferringAppAutofillServerHeuristic},
        {kRepeatSiteVisit, kChrome, kNoDetectionHeuristic,
         kRepeatSiteVisitChromeReferringAppNoDetectionHeuristic},
        {kRepeatSiteVisit, kChrome, kAutofillLocal,
         kRepeatSiteVisitChromeReferringAppAutofillLocalHeuristic},
        {kRepeatSiteVisit, kChrome, kAutofillServer,
         kRepeatSiteVisitChromeReferringAppAutofillServerHeuristic},
        {kRepeatSiteVisit, kSmsApp, kNoDetectionHeuristic,
         kRepeatSiteVisitSmsReferringAppNoDetectionHeuristic},
        {kRepeatSiteVisit, kSmsApp, kAutofillLocal,
         kRepeatSiteVisitSmsReferringAppAutofillLocalHeuristic},
        {kRepeatSiteVisit, kSmsApp, kAutofillServer,
         kRepeatSiteVisitSmsReferringAppAutofillServerHeuristic},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    GetCreditCardFormEventTest,
    testing::ValuesIn(get_credit_card_form_event_test_cases),
    GetCreditCardFormEventTestCase::GetTestName);

TEST_P(GetCreditCardFormEventTest, GetExpectedEvent) {
  const GetCreditCardFormEventTestCase& test_case = GetParam();
  CreditCardFormEvent event = GetCreditCardFormEvent(
      test_case.site_visit, test_case.referring_app, test_case.heuristic);
  ASSERT_EQ(event, test_case.expected_event);
}

TEST(CreditCardFormEventTest, LogEvent) {
  base::HistogramTester histogram_tester;
  CreditCardFormEvent expected_event = GetCreditCardFormEvent(
      kUnknownSiteVisit, kNoReferringApp, kNoDetectionHeuristic);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.CreditCardFormEvent.OnFieldTypesDetermined", 0);
  LogEvent("OnFieldTypesDetermined");
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.CreditCardFormEvent.OnFieldTypesDetermined",
      expected_event, 1);
}

}  // namespace safe_browsing::credit_card_form

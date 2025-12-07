// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/credit_card_form_event.h"

#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/safe_browsing/core/browser/referring_app_info.h"
#endif

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
    case kOtherApp:
      return "OtherApp";
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
        {kUnknownSiteVisit, kOtherApp, kNoDetectionHeuristic,
         kUnknownSiteVisitOtherReferringAppNoDetectionHeuristic},
        {kUnknownSiteVisit, kOtherApp, kAutofillLocal,
         kUnknownSiteVisitOtherReferringAppAutofillLocalHeuristic},
        {kUnknownSiteVisit, kOtherApp, kAutofillServer,
         kUnknownSiteVisitOtherReferringAppAutofillServerHeuristic},
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
        {kNewSiteVisit, kOtherApp, kNoDetectionHeuristic,
         kNewSiteVisitOtherReferringAppNoDetectionHeuristic},
        {kNewSiteVisit, kOtherApp, kAutofillLocal,
         kNewSiteVisitOtherReferringAppAutofillLocalHeuristic},
        {kNewSiteVisit, kOtherApp, kAutofillServer,
         kNewSiteVisitOtherReferringAppAutofillServerHeuristic},
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
        {kRepeatSiteVisit, kOtherApp, kNoDetectionHeuristic,
         kRepeatSiteVisitOtherReferringAppNoDetectionHeuristic},
        {kRepeatSiteVisit, kOtherApp, kAutofillLocal,
         kRepeatSiteVisitOtherReferringAppAutofillLocalHeuristic},
        {kRepeatSiteVisit, kOtherApp, kAutofillServer,
         kRepeatSiteVisitOtherReferringAppAutofillServerHeuristic},
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

#if BUILDFLAG(IS_ANDROID)
struct ReferringAppTestCase {
  const char* name;
  const char* referring_app_name;
  ReferringApp referring_app;

  static std::string GetTestName(
      const testing::TestParamInfo<ReferringAppTestCase>& test_case) {
    return test_case.param.name;
  }
};

class ReferringAppTest : public testing::TestWithParam<ReferringAppTestCase> {};

constexpr ReferringAppTestCase referring_app_test_cases[] = {
    {"no_referring_app", nullptr, kNoReferringApp},
    {"empty_referring_app_name", "", kNoReferringApp},
    {"some_other_app", "com.bar.foo", kOtherApp},
    {"some_other_app_uri", "android-app://com.bar.foo", kOtherApp},
    {"chrome", "chrome", kChrome},
    {"chrome_ui", "android-app://chrome", kChrome},
    {"android_messages", "android.messages", kSmsApp},
    {"android_messages_uri", "android-app://android.messages", kSmsApp},
    {"samsung_messaging", "com.samsung.android.messaging", kSmsApp},
    {"samsung_messaging_uri", "android-app://com.samsung.android.messaging",
     kSmsApp},
};

INSTANTIATE_TEST_SUITE_P(All,
                         ReferringAppTest,
                         testing::ValuesIn(referring_app_test_cases),
                         ReferringAppTestCase::GetTestName);

TEST_P(ReferringAppTest, FromReferrinugAppInfo) {
  const ReferringAppTestCase& test_case = GetParam();
  internal::ReferringAppInfo referring_app_info{};
  if (test_case.referring_app_name) {
    referring_app_info.referring_app_name = test_case.referring_app_name;
  }
  ASSERT_EQ(test_case.referring_app, FromReferringAppInfo(referring_app_info));
}
#endif

struct LogEventTestCase {
  SiteVisit site_visit;
  FieldDetectionHeuristic field_heuristic;
  CreditCardFormEvent expected_event;

  static std::string GetTestName(
      const testing::TestParamInfo<LogEventTestCase>& test_case) {
    return base::StrCat({
        ToString(test_case.param.site_visit),
        ToString(test_case.param.field_heuristic),
    });
  }
};

class LogEventTest : public testing::TestWithParam<LogEventTestCase> {};

std::vector<LogEventTestCase> GetLogEventTestCases() {
  std::vector<LogEventTestCase> cases;
  for (int sv = 0; sv <= kSiteVisitMaxValue; sv++) {
    SiteVisit site_visit = static_cast<SiteVisit>(sv);
    for (int fdh = 0; fdh <= kFieldDetectionHeuristicMaxValue; fdh++) {
      FieldDetectionHeuristic field_heuristic =
          static_cast<FieldDetectionHeuristic>(fdh);
      CreditCardFormEvent event =
          GetCreditCardFormEvent(site_visit, kNoReferringApp, field_heuristic);
      LogEventTestCase test_case{site_visit, field_heuristic, event};
      cases.emplace_back(test_case);
    }
  }
  return cases;
}

INSTANTIATE_TEST_SUITE_P(All,
                         LogEventTest,
                         testing::ValuesIn(GetLogEventTestCases()),
                         LogEventTestCase::GetTestName);

TEST_P(LogEventTest, LogEventLogsExpectedHistogram) {
  const LogEventTestCase& test_case = GetParam();
  SiteVisit site_visit = test_case.site_visit;
  FieldDetectionHeuristic field_heuristic = test_case.field_heuristic;
  CreditCardFormEvent expected_event = test_case.expected_event;
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.CreditCardFormEvent2.OnFieldTypesDetermined", 0);
  LogEvent("OnFieldTypesDetermined", site_visit, field_heuristic);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.CreditCardFormEvent2.OnFieldTypesDetermined",
      expected_event, 1);
}

}  // namespace safe_browsing::credit_card_form

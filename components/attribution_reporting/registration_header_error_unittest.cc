// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_header_error.h"

#include "base/test/values_test_util.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

TEST(RegistrationHeaderErrorTest, SourceRegistrationErrorDetails) {
  const struct {
    SourceRegistrationError error;
    const char* expected_json;
  } kTestCases[] = {
      {
          SourceRegistrationError::kInvalidJson,
          R"json({
            "msg": "invalid JSON"
          })json",
      },
      {
          SourceRegistrationError::kRootWrongType,
          R"json({
            "path": [],
            "msg": "must be a dictionary"
          })json",
      },
      {
          SourceRegistrationError::kDestinationMissing,
          R"json({
            "path": ["destination"],
            "msg": "required"
          })json",
      },
      {
          SourceRegistrationError::kDestinationWrongType,
          R"json({
            "path": ["destination"],
            "msg": "must be a string or a list of 1-3 strings"
          })json",
      },
      {
          SourceRegistrationError::kDestinationUntrustworthy,
          R"json({
            "path": ["destination"],
            "msg": "must be a potentially trustworthy URL that uses HTTP/HTTPS"
          })json",
      },
      {
          SourceRegistrationError::kDestinationListUntrustworthy,
          R"json({
            "path": ["destination", "*"],
            "msg": "must be a potentially trustworthy URL that uses HTTP/HTTPS"
          })json",
      },
      {
          SourceRegistrationError::kFilterDataKeyTooLong,
          R"json({
            "path": ["filter_data"],
            "msg": "key length must be less than or equal to 25"
          })json",
      },
      {
          SourceRegistrationError::kFilterDataKeyReserved,
          R"json({
            "path": ["filter_data"],
            "msg": "\"source_type\" and keys starting with \"_\" are reserved"
          })json",
      },
      {
          SourceRegistrationError::kFilterDataDictInvalid,
          R"json({
            "path": ["filter_data"],
            "msg": "must be a dictionary with size less than or equal to 50"
          })json",
      },
      {
          SourceRegistrationError::kFilterDataListInvalid,
          R"json({
            "path": ["filter_data", "*"],
            "msg": "must be a list whose length is in the range [0, 50]"
          })json",
      },
      {
          SourceRegistrationError::kFilterDataListValueInvalid,
          R"json({
            "path": ["filter_data", "*", "*"],
            "msg": "must be a string with length less than or equal to 25"
          })json",
      },
      {
          SourceRegistrationError::kAggregationKeysKeyTooLong,
          R"json({
            "path": ["aggregation_keys"],
            "msg": "key length must be less than or equal to 25"
          })json",
      },
      {
          SourceRegistrationError::kAggregationKeysDictInvalid,
          R"json({
            "path": ["aggregation_keys"],
            "msg": "must be a dictionary with size less than or equal to 20",
          })json",
      },
      {
          SourceRegistrationError::kAggregationKeysValueInvalid,
          R"json({
            "path": ["aggregation_keys", "*"],
            "msg": "must be a base16-encoded string of a uint128 with a \"0x\" prefix"
          })json",
      },
      {
          SourceRegistrationError::kSourceEventIdValueInvalid,
          R"json({
            "path": ["source_event_id"],
            "msg": "must be a base10-encoded string of a uint64"
          })json",
      },
      {
          SourceRegistrationError::kPriorityValueInvalid,
          R"json({
            "path": ["priority"],
            "msg": "must be a base10-encoded string of a int64"
          })json",
      },
      {
          SourceRegistrationError::kExpiryValueInvalid,
          R"json({
            "path": ["expiry"],
            "msg": "must be a non-negative integer or a base10-encoded string of a uint64"
          })json",
      },
      {
          SourceRegistrationError::kEventReportWindowValueInvalid,
          R"json({
            "path": ["event_report_window"],
            "msg": "must be a non-negative integer or a base10-encoded string of a uint64"
          })json",
      },
      {
          SourceRegistrationError::kAggregatableReportWindowValueInvalid,
          R"json({
            "path": ["aggregatable_report_window"],
            "msg": "must be a non-negative integer or a base10-encoded string of a uint64"
          })json",
      },
      {
          SourceRegistrationError::kMaxEventLevelReportsValueInvalid,
          R"json({
            "path": ["max_event_level_reports"],
            "msg": "must be a non-negative integer and less than or equal to 20"
          })json",
      },
      {
          SourceRegistrationError::kEventReportWindowsWrongType,
          R"json({
            "path": ["event_report_windows"],
            "msg": "must be a dictionary"
          })json",
      },
      {
          SourceRegistrationError::kEventReportWindowsEndTimesMissing,
          R"json({
            "path": ["event_report_windows", "end_times"],
            "msg": "required"
          })json",
      },
      {
          SourceRegistrationError::kEventReportWindowsEndTimeDurationLTEStart,
          R"json({
            "path": ["event_report_windows", "end_times", "*"],
            "msg": "must be greater than start_time and greater than previous end_time"
          })json",
      },
      {
          SourceRegistrationError::kBothEventReportWindowFieldsFound,
          R"json({
            "path": [],
            "msg": "mutually exclusive fields: event_report_window, event_report_windows"
          })json",
      },
      {
          SourceRegistrationError::kEventReportWindowsEndTimesListInvalid,
          R"json({
            "path": ["event_report_windows", "end_times"],
            "msg": "must be a list whose length is in the range [1, 5]"
          })json",
      },
      {
          SourceRegistrationError::kEventReportWindowsStartTimeInvalid,
          R"json({
            "path": ["event_report_windows", "start_time"],
            "msg": "must be a non-negative integer and less than or equal to expiry"
          })json",
      },
      {
          SourceRegistrationError::kEventReportWindowsEndTimeValueInvalid,
          R"json({
            "path": ["event_report_windows", "end_times"],
            "msg": "must be a positive integer"
          })json",
      },
      {
          SourceRegistrationError::kTriggerDataMatchingValueInvalid,
          R"json({
            "path": ["trigger_data_matching"],
            "msg": "must be one of the following (case-sensitive): exact, modulus"
          })json",
      },
      {
          SourceRegistrationError::kTriggerSpecsWrongType,
          R"json({
            "path": ["trigger_specs"],
            "msg": "must be a list of dictionaries"
          })json",
      },
      {
          SourceRegistrationError::kTriggerSpecTriggerDataMissing,
          R"json({
            "path": ["trigger_specs", "*", "trigger_data"],
            "msg": "required"
          })json",
      },
      {
          SourceRegistrationError::kTriggerDataListInvalid,
          R"json({
            "path": ["trigger_data"],
            "msg": "must be a list of non-negative integers in the range [0, 4294967295]"
          })json",
      },
      {
          SourceRegistrationError::kTriggerSpecTriggerDataListInvalid,
          R"json({
            "path": ["trigger_specs", "*", "trigger_data"],
            "msg": "must be a list of non-negative integers in the range [0, 4294967295]"
          })json",
      },
      {
          SourceRegistrationError::kDuplicateTriggerData,
          R"json({
            "path": ["trigger_data"],
            "msg": "must not contain duplicate trigger_data"
          })json",
      },
      {
          SourceRegistrationError::kTriggerSpecDuplicateTriggerData,
          R"json({
            "path": ["trigger_specs"],
            "msg": "must not contain duplicate trigger_data"
          })json",
      },
      {
          SourceRegistrationError::kExcessiveTriggerData,
          R"json({
            "path": ["trigger_data"],
            "msg": "must not exceed a maximum of 32 distinct trigger_data values"
          })json",
      },
      {
          SourceRegistrationError::kTriggerSpecExcessiveTriggerData,
          R"json({
            "path": ["trigger_specs"],
            "msg": "must not exceed a maximum of 32 distinct trigger_data values"
          })json",
      },
      {
          SourceRegistrationError::kInvalidTriggerDataForMatchingMode,
          R"json({
            "path": ["trigger_data_matching"],
            "msg": "trigger_data must form a contiguous sequence of integers starting at 0 for modulus"
          })json",
      },
      {
          SourceRegistrationError::kTopLevelTriggerDataAndTriggerSpecs,
          R"json({
            "path": [],
            "msg": "mutually exclusive fields: trigger_data, trigger_specs"
          })json",
      },
      {
          SourceRegistrationError::kSummaryWindowOperatorValueInvalid,
          R"json({
            "path": ["trigger_specs", "*", "summary_window_operator"],
            "msg": "must be one of the following (case-sensitive): count, value_sum"
          })json",
      },
      {
          SourceRegistrationError::kSummaryBucketsListInvalid,
          R"json({
            "path": ["trigger_specs", "*", "summary_buckets"],
            "msg": "must be a list whose length is in the range [1, max_event_level_reports]"
          })json",
      },
      {
          SourceRegistrationError::kSummaryBucketsValueInvalid,
          R"json({
            "path": ["trigger_specs", "*", "summary_buckets", "*"],
            "msg": "must be a non-negative integer and greater than previous value and less than or equal to 4294967295"
          })json",
      },
      {
          SourceRegistrationError::kEventLevelEpsilonValueInvalid,
          R"json({
            "path": ["event_level_epsilon"],
            "msg": "must be a number in the range [0, 14]"
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.error);
    EXPECT_THAT(ErrorDetails(test_case.error),
                base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting

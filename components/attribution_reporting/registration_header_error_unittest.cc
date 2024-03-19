// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_header_error.h"

#include "base/test/values_test_util.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/os_registration_error.mojom-shared.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::OsRegistrationError;
using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::TriggerRegistrationError;

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
    RegistrationHeaderError error(/*header_value=*/"", test_case.error);
    EXPECT_THAT(error.ErrorDetails(),
                base::test::IsJson(test_case.expected_json));
  }
}

TEST(RegistrationHeaderErrorTest, TriggerRegistrationErrorDetails) {
  const struct {
    TriggerRegistrationError error;
    const char* expected_json;
  } kTestCases[] = {
      {
          TriggerRegistrationError::kInvalidJson,
          R"json({
            "msg": "invalid JSON"
          })json",
      },
      {
          TriggerRegistrationError::kRootWrongType,
          R"json({
            "path": [],
            "msg": "must be a dictionary"
          })json",
      },
      {
          TriggerRegistrationError::kFiltersWrongType,
          R"json({
            "path": ["filters"],
            "msg": "must be a dictionary or a list of dictionaries"
          })json",
      },
      {
          TriggerRegistrationError::kFiltersValueInvalid,
          R"json({
            "path": ["filters", "*"],
            "msg": "must be a list of strings"
          })json",
      },
      {
          TriggerRegistrationError::kFiltersLookbackWindowValueInvalid,
          R"json({
            "path": ["filters", "_lookback_window"],
            "msg": "must be a positive integer"
          })json",
      },
      {
          TriggerRegistrationError::kFiltersUsingReservedKey,
          R"json({
            "path": ["filters"],
            "msg": "strings starting with \"_\" are reserved keys"
          })json",
      },
      {
          TriggerRegistrationError::kFiltersListValueInvalid,
          R"json({
            "path": ["filters", "*", "*"],
            "msg": "must be a list of strings"
          })json",
      },
      {
          TriggerRegistrationError::kFiltersListLookbackWindowValueInvalid,
          R"json({
            "path": ["filters", "*", "_lookback_window"],
            "msg": "must be a positive integer"
          })json",
      },
      {
          TriggerRegistrationError::kFiltersListUsingReservedKey,
          R"json({
            "path": ["filters", "*"],
            "msg": "strings starting with \"_\" are reserved keys"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableValuesWrongType,
          R"json({
            "path": ["aggregatable_values"],
            "msg": "must be a dictionary or a list of dictionaries"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableValuesKeyTooLong,
          R"json({
            "path": ["aggregatable_values"],
            "msg": "key length must be less than or equal to 25"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableValuesListValuesFieldMissing,
          R"json({
            "path": ["aggregatable_values", "*", "values"],
            "msg": "required"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableValuesListKeyTooLong,
          R"json({
            "path": ["aggregatable_values", "*", "values"],
            "msg": "key length must be less than or equal to 25"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableValuesValueInvalid,
          R"json({
            "path": ["aggregatable_values", "*"],
            "msg": "must be an integer in the range [1, 65536]"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableValuesListValueInvalid,
          R"json({
            "path": ["aggregatable_values", "*", "values", "*"],
            "msg": "must be an integer in the range [1, 65536]"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableTriggerDataWrongType,
          R"json({
            "path": ["aggregatable_trigger_data"],
            "msg": "must be a list of dictionaries"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableTriggerDataKeyPieceMissing,
          R"json({
            "path": ["aggregatable_trigger_data", "*", "key_piece"],
            "msg": "required"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableTriggerDataSourceKeysInvalid,
          R"json({
            "path": ["aggregatable_trigger_data", "*", "source_keys"],
            "msg": "must be a list of strings, each with length less than or equal to 25"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableTriggerDataKeyPieceInvalid,
          R"json({
            "path": ["aggregatable_trigger_data", "*", "key_piece"],
            "msg": "must be a base16-encoded string of a uint128 with a \"0x\" prefix"
          })json",
      },
      {
          TriggerRegistrationError::kEventTriggerDataWrongType,
          R"json({
            "path": ["event_trigger_data"],
            "msg": "must be a list of dictionaries"
          })json",
      },
      {
          TriggerRegistrationError::kEventTriggerDataValueInvalid,
          R"json({
            "path": ["event_trigger_data", "*", "trigger_data"],
            "msg": "must be a base10-encoded string of a uint64"
          })json",
      },
      {
          TriggerRegistrationError::kEventPriorityValueInvalid,
          R"json({
            "path": ["event_trigger_data", "*", "priority"],
            "msg": "must be a base10-encoded string of a int64"
          })json",
      },
      {
          TriggerRegistrationError::kEventDedupKeyValueInvalid,
          R"json({
            "path": ["event_trigger_data", "*", "deduplication_key"],
            "msg": "must be a base10-encoded string of a uint64"
          })json",
      },
      {
          TriggerRegistrationError::kAggregationCoordinatorValueInvalid,
          R"json({
            "path": ["aggregation_coordinator_origin"],
            "msg": "must be a potentially trustworthy URL on the allowlist that uses HTTP/HTTPS"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableDedupKeyValueInvalid,
          R"json({
            "path": ["aggregatable_deduplication_keys", "*", "deduplication_key"],
            "msg": "must be a base10-encoded string of a uint64"
          })json",
      },
      {
          TriggerRegistrationError::kAggregatableDedupKeyWrongType,
          R"json({
            "path": ["aggregatable_deduplication_keys"],
            "msg": "must be a list of dictionaries"
          })json",
      },
      {
          TriggerRegistrationError::
              kAggregatableSourceRegistrationTimeValueInvalid,
          R"json({
            "path": ["aggregatable_source_registration_time"],
            "msg": "must be one of the following (case-sensitive): include, exclude"
          })json",
      },
      {
          TriggerRegistrationError::kTriggerContextIdInvalidValue,
          R"json({
            "path": ["trigger_context_id"],
            "msg": "must be a non-empty string with length less than or equal to 64"
          })json",
      },
      {
          TriggerRegistrationError::
              kTriggerContextIdInvalidSourceRegistrationTimeConfig,
          R"json({
            "path": ["trigger_context_id"],
            "msg": "is prohibited for aggregatable_source_registration_time include"
          })json",
      },
      {
          TriggerRegistrationError::kEventValueInvalid,
          R"json({
            "path": ["event_trigger_data", "*", "value"],
            "msg": "must be a positive integer in the range [1, 4294967295]"
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.error);

    RegistrationHeaderError error(/*header_value=*/"", test_case.error);
    EXPECT_THAT(error.ErrorDetails(),
                base::test::IsJson(test_case.expected_json));
  }
}

TEST(RegistrationHeaderErrorTest, OsRegistrationError) {
  const struct {
    OsRegistrationError error;
    const char* expected_json;
  } kTestCases[] = {
      {
          OsRegistrationError::kInvalidList,
          R"json({
            "msg": "must be a list of URLs"
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.error);
    RegistrationHeaderError os_source_error(
        /*header_value=*/"", OsSourceRegistrationError(test_case.error));
    EXPECT_THAT(os_source_error.ErrorDetails(),
                base::test::IsJson(test_case.expected_json));
    RegistrationHeaderError os_trigger_error(
        /*header_value=*/"", OsTriggerRegistrationError(test_case.error));
    EXPECT_THAT(os_trigger_error.ErrorDetails(),
                base::test::IsJson(test_case.expected_json));
  }
}

TEST(RegistrationHeaderErrorTest, HeaderName) {
  const struct {
    RegistrationHeaderErrorDetails error;
    const char* expected;
  } kTestCases[] = {
      {
          SourceRegistrationError::kInvalidJson,
          kAttributionReportingRegisterSourceHeader,
      },
      {
          TriggerRegistrationError::kInvalidJson,
          kAttributionReportingRegisterTriggerHeader,
      },
      {
          OsSourceRegistrationError(OsRegistrationError::kInvalidList),
          kAttributionReportingRegisterOsSourceHeader,
      },
      {
          OsTriggerRegistrationError(OsRegistrationError::kInvalidList),
          kAttributionReportingRegisterOsTriggerHeader,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.expected);
    RegistrationHeaderError error(/*header_value=*/"", test_case.error);
    EXPECT_EQ(error.HeaderName(), test_case.expected);
  }
}

}  // namespace
}  // namespace attribution_reporting

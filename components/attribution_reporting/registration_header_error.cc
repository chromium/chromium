// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_header_error.h"

#include <stdint.h>

#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/registration_header_type.mojom-shared.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::RegistrationHeaderType;
using ::attribution_reporting::mojom::SourceRegistrationError;

base::Value GetPath(base::span<const std::string_view> parts) {
  base::Value::List list;
  list.reserve(parts.size());
  for (const std::string_view part : parts) {
    list.Append(part);
  }
  return base::Value(std::move(list));
}

base::Value SerializeErrorDetails(base::Value path, std::string msg) {
  if (msg.empty()) {
    // Should only be possible with compromised renderers.
    return base::Value();
  }

  base::Value::Dict dict;
  if (path.is_list()) {
    dict.Set("path", std::move(path));
  }
  dict.Set("msg", std::move(msg));
  return base::Value(std::move(dict));
}

std::string MutuallyExclusiveMsg(base::span<const std::string_view> fields) {
  return base::StrCat(
      {"mutually exclusive fields: ", base::JoinString(fields, ", ")});
}

std::string ListLengthMsg(size_t min, std::string_view max) {
  return base::StrCat({"must be a list whose length is in the range [",
                       base::NumberToString(min), ", ", max, "]"});
}

std::string DictionarySizeMsg(size_t max) {
  return base::StrCat({"must be a dictionary with size less than or equal to ",
                       base::NumberToString(max)});
}

std::string EnumMsg(base::span<const std::string_view> fields) {
  return base::StrCat({"must be one of the following (case-sensitive): ",
                       base::JoinString(fields, ", ")});
}

std::string ExcessiveTriggerDataMsg() {
  return base::StrCat({"must not exceed a maximum of ",
                       base::NumberToString(kMaxTriggerDataPerSource),
                       " distinct trigger_data values"});
}

std::string InvalidTriggerDataMsg() {
  return base::StrCat(
      {"must be a list of non-negative integers in the range [0, ",
       base::NumberToString(std::numeric_limits<uint32_t>::max()), "]"});
}

}  // namespace

base::Value ErrorDetails(SourceRegistrationError error) {
  static constexpr char kIndex[] = "*";

  static constexpr char kBase10EncodedMsg[] =
      "must be a base10-encoded string of a ";
  static constexpr char kDestinationPotentiallyTrustworthyMsg[] =
      "must be a potentially trustworthy URL that uses HTTP/HTTPS";
  static constexpr char kDictionaryMsg[] = "must be a dictionary";
  static constexpr char kDuplicateTriggerDataMsg[] =
      "must not contain duplicate trigger_data";
  static constexpr char kExpiryOrReportWindowInvalidMsg[] =
      "must be a non-negative integer or a base10-encoded string of a uint64";
  static constexpr char kKeyLengthMsg[] =
      "key length must be less than or equal to ";
  static constexpr char kRequiredMsg[] = "required";

  base::Value path;
  std::string msg;

  switch (error) {
    case SourceRegistrationError::kInvalidJson:
      msg = "invalid JSON";
      break;
    case SourceRegistrationError::kRootWrongType:
      path = base::Value(base::Value::List());
      msg = kDictionaryMsg;
      break;
    case SourceRegistrationError::kDestinationMissing:
      path = GetPath({{kDestination}});
      msg = kRequiredMsg;
      break;
    case SourceRegistrationError::kDestinationWrongType:
      path = GetPath({{kDestination}});
      msg = base::StrCat({"must be a string or a list of 1-",
                          base::NumberToString(kMaxDestinations), " strings"});
      break;
    case SourceRegistrationError::kDestinationUntrustworthy:
      path = GetPath({{kDestination}});
      msg = kDestinationPotentiallyTrustworthyMsg;
      break;
    case SourceRegistrationError::kDestinationListUntrustworthy:
      path = GetPath({{kDestination, kIndex}});
      msg = kDestinationPotentiallyTrustworthyMsg;
      break;
    case SourceRegistrationError::kFilterDataKeyTooLong:
      path = GetPath({{kFilterData}});
      msg = base::StrCat(
          {kKeyLengthMsg, base::NumberToString(kMaxBytesPerFilterString)});
      break;
    case SourceRegistrationError::kFilterDataKeyReserved:
      path = GetPath({{kFilterData}});
      msg = R"("source_type" and keys starting with "_" are reserved)";
      break;
    case SourceRegistrationError::kFilterDataDictInvalid:
      path = GetPath({{kFilterData}});
      msg = DictionarySizeMsg(kMaxFiltersPerSource);
      break;
    case SourceRegistrationError::kFilterDataListInvalid:
      path = GetPath({{kFilterData, kIndex}});
      msg = ListLengthMsg(0, base::NumberToString(kMaxValuesPerFilter));
      break;
    case SourceRegistrationError::kFilterDataListValueInvalid:
      path = GetPath({{kFilterData, kIndex, kIndex}});
      msg = base::StrCat({"must be a string with length less than or equal to ",
                          base::NumberToString(kMaxBytesPerFilterString)});
      break;
    case SourceRegistrationError::kAggregationKeysKeyTooLong:
      path = GetPath({{kAggregationKeys}});
      msg = base::StrCat(
          {kKeyLengthMsg, base::NumberToString(kMaxBytesPerAggregationKeyId)});
      break;
    case SourceRegistrationError::kAggregationKeysDictInvalid:
      path = GetPath({{kAggregationKeys}});
      msg = DictionarySizeMsg(kMaxAggregationKeysPerSource);
      break;
    case SourceRegistrationError::kAggregationKeysValueInvalid:
      path = GetPath({{kAggregationKeys, kIndex}});
      msg =
          R"(must be a base16-encoded string of a uint128 with a "0x" prefix)";
      break;
    case SourceRegistrationError::kSourceEventIdValueInvalid:
      path = GetPath({{kSourceEventId}});
      msg = base::StrCat({kBase10EncodedMsg, "uint64"});
      break;
    case SourceRegistrationError::kPriorityValueInvalid:
      path = GetPath({{kPriority}});
      msg = base::StrCat({kBase10EncodedMsg, "int64"});
      break;
    case SourceRegistrationError::kExpiryValueInvalid:
      path = GetPath({{kExpiry}});
      msg = kExpiryOrReportWindowInvalidMsg;
      break;
    case SourceRegistrationError::kEventReportWindowValueInvalid:
      path = GetPath({{kEventReportWindow}});
      msg = kExpiryOrReportWindowInvalidMsg;
      break;
    case SourceRegistrationError::kAggregatableReportWindowValueInvalid:
      path = GetPath({{kAggregatableReportWindow}});
      msg = kExpiryOrReportWindowInvalidMsg;
      break;
    case SourceRegistrationError::kMaxEventLevelReportsValueInvalid:
      path = GetPath({{kMaxEventLevelReports}});
      msg = base::StrCat(
          {"must be a non-negative integer and less than or equal to ",
           base::NumberToString(kMaxSettableEventLevelAttributionsPerSource)});
      break;
    case SourceRegistrationError::kEventReportWindowsWrongType:
      path = GetPath({{kEventReportWindows}});
      msg = kDictionaryMsg;
      break;
    case SourceRegistrationError::kEventReportWindowsEndTimesMissing:
      path = GetPath({{kEventReportWindows, kEndTimes}});
      msg = kRequiredMsg;
      break;
    case SourceRegistrationError::kEventReportWindowsEndTimeDurationLTEStart:
      path = GetPath({{kEventReportWindows, kEndTimes, kIndex}});
      msg =
          "must be greater than start_time and greater than previous end_time";
      break;
    case SourceRegistrationError::kBothEventReportWindowFieldsFound:
      path = base::Value(base::Value::List());
      msg = MutuallyExclusiveMsg({{kEventReportWindow, kEventReportWindows}});
      break;
    case SourceRegistrationError::kEventReportWindowsEndTimesListInvalid:
      path = GetPath({{kEventReportWindows, kEndTimes}});
      msg = ListLengthMsg(1, base::NumberToString(kMaxEventLevelReportWindows));
      break;
    case SourceRegistrationError::kEventReportWindowsStartTimeInvalid:
      path = GetPath({{kEventReportWindows, kStartTime}});
      msg = base::StrCat(
          {"must be a non-negative integer and less than or equal to ",
           kExpiry});
      break;
    case SourceRegistrationError::kEventReportWindowsEndTimeValueInvalid:
      path = GetPath({{kEventReportWindows, kEndTimes}});
      msg = "must be a positive integer";
      break;
    case SourceRegistrationError::kTriggerDataMatchingValueInvalid:
      path = GetPath({{kTriggerDataMatching}});
      msg = EnumMsg({{kTriggerDataMatchingExact, kTriggerDataMatchingModulus}});
      break;
    case SourceRegistrationError::kTriggerSpecsWrongType:
      path = GetPath({{kTriggerSpecs}});
      msg = "must be a list of dictionaries";
      break;
    case SourceRegistrationError::kTriggerSpecTriggerDataMissing:
      path = GetPath({{kTriggerSpecs, kIndex, kTriggerData}});
      msg = kRequiredMsg;
      break;
    case SourceRegistrationError::kTriggerDataListInvalid:
      path = GetPath({{kTriggerData}});
      msg = InvalidTriggerDataMsg();
      break;
    case SourceRegistrationError::kTriggerSpecTriggerDataListInvalid:
      path = GetPath({{kTriggerSpecs, kIndex, kTriggerData}});
      msg = InvalidTriggerDataMsg();
      break;
    case SourceRegistrationError::kDuplicateTriggerData:
      path = GetPath({{kTriggerData}});
      msg = kDuplicateTriggerDataMsg;
      break;
    case SourceRegistrationError::kTriggerSpecDuplicateTriggerData:
      path = GetPath({{kTriggerSpecs}});
      msg = kDuplicateTriggerDataMsg;
      break;
    case SourceRegistrationError::kExcessiveTriggerData:
      path = GetPath({{kTriggerData}});
      msg = ExcessiveTriggerDataMsg();
      break;
    case SourceRegistrationError::kTriggerSpecExcessiveTriggerData:
      path = GetPath({{kTriggerSpecs}});
      msg = ExcessiveTriggerDataMsg();
      break;
    case SourceRegistrationError::kInvalidTriggerDataForMatchingMode:
      path = GetPath({{kTriggerDataMatching}});
      msg = base::StrCat(
          {"trigger_data must form a contiguous sequence of integers starting "
           "at 0 for ",
           kTriggerDataMatchingModulus});
      break;
    case SourceRegistrationError::kTopLevelTriggerDataAndTriggerSpecs:
      path = base::Value(base::Value::List());
      msg = MutuallyExclusiveMsg({{kTriggerData, kTriggerSpecs}});
      break;
    case SourceRegistrationError::kSummaryWindowOperatorValueInvalid:
      path = GetPath({{kTriggerSpecs, kIndex, kSummaryWindowOperator}});
      msg = EnumMsg(
          {{kSummaryWindowOperatorCount, kSummaryWindowOperatorValueSum}});
      break;
    case SourceRegistrationError::kSummaryBucketsListInvalid:
      path = GetPath({{kTriggerSpecs, kIndex, kSummaryBuckets}});
      msg = ListLengthMsg(1, "max_event_level_reports");
      break;
    case SourceRegistrationError::kSummaryBucketsValueInvalid:
      path = GetPath({{kTriggerSpecs, kIndex, kSummaryBuckets, kIndex}});
      msg = base::StrCat(
          {"must be a non-negative integer and greater than previous value and "
           "less than or equal to ",
           base::NumberToString(std::numeric_limits<uint32_t>::max())});
      break;
    case SourceRegistrationError::kEventLevelEpsilonValueInvalid:
      path = GetPath({{kEventLevelEpsilon}});
      msg = base::StrCat({"must be a number in the range [0, ",
                          base::NumberToString(EventLevelEpsilon::max()), "]"});
      break;
  }

  return SerializeErrorDetails(std::move(path), std::move(msg));
}

}  // namespace attribution_reporting

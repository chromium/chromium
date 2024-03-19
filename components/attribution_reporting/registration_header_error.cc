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
#include "base/functional/overloaded.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/os_registration_error.mojom-shared.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::OsRegistrationError;
using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::TriggerRegistrationError;

constexpr char kIndex[] = "*";

constexpr char kAggregationKeyPieceMsg[] =
    R"(must be a base16-encoded string of a uint128 with a "0x" prefix)";
constexpr char kBase10EncodedMsg[] = "must be a base10-encoded string of a ";
constexpr char kDictionaryMsg[] = "must be a dictionary";
constexpr char kInvalidJsonMsg[] = "invalid JSON";
constexpr char kListOfDictionariesMsg[] = "must be a list of dictionaries";
constexpr char kKeyLengthMsg[] = "key length must be less than or equal to ";
constexpr char kOsInvalidListMsg[] = "must be a list of URLs";
constexpr char kPositiveIntegerMsg[] = "must be a positive integer";
constexpr char kRequiredMsg[] = "required";

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

std::string AggregationKeyTooLongMsg() {
  return base::StrCat(
      {kKeyLengthMsg, base::NumberToString(kMaxBytesPerAggregationKeyId)});
}

std::string AggregatableValueMsg() {
  return base::StrCat({"must be an integer in the range [1, ",
                       base::NumberToString(kMaxAggregatableValue), "]"});
}

base::Value ErrorDetails(OsRegistrationError error) {
  std::string msg;
  switch (error) {
    case OsRegistrationError::kInvalidList:
      msg = kOsInvalidListMsg;
      break;
  }

  return SerializeErrorDetails(/*path=*/base::Value(), std::move(msg));
}

base::Value ErrorDetails(SourceRegistrationError error) {
  static constexpr char kDestinationPotentiallyTrustworthyMsg[] =
      "must be a potentially trustworthy URL that uses HTTP/HTTPS";
  static constexpr char kDuplicateTriggerDataMsg[] =
      "must not contain duplicate trigger_data";
  static constexpr char kExpiryOrReportWindowInvalidMsg[] =
      "must be a non-negative integer or a base10-encoded string of a uint64";

  base::Value path;
  std::string msg;

  switch (error) {
    case SourceRegistrationError::kInvalidJson:
      msg = kInvalidJsonMsg;
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
      msg = kAggregationKeyPieceMsg;
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
      msg = kPositiveIntegerMsg;
      break;
    case SourceRegistrationError::kTriggerDataMatchingValueInvalid:
      path = GetPath({{kTriggerDataMatching}});
      msg = EnumMsg({{kTriggerDataMatchingExact, kTriggerDataMatchingModulus}});
      break;
    case SourceRegistrationError::kTriggerSpecsWrongType:
      path = GetPath({{kTriggerSpecs}});
      msg = kListOfDictionariesMsg;
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

base::Value ErrorDetails(TriggerRegistrationError error) {
  static constexpr char kDictionaryOrListOfDictionariesMsg[] =
      "must be a dictionary or a list of dictionaries";
  static constexpr char kFiltersReservedKeysMsg[] =
      R"(strings starting with "_" are reserved keys)";
  static constexpr char kFiltersValueInvalidMsg[] = "must be a list of strings";

  base::Value path;
  std::string msg;

  switch (error) {
    case TriggerRegistrationError::kInvalidJson:
      msg = kInvalidJsonMsg;
      break;
    case TriggerRegistrationError::kRootWrongType:
      path = base::Value(base::Value::List());
      msg = kDictionaryMsg;
      break;
    case TriggerRegistrationError::kFiltersWrongType:
      path = GetPath({{kFilters}});
      msg = kDictionaryOrListOfDictionariesMsg;
      break;
    case TriggerRegistrationError::kFiltersValueInvalid:
      path = GetPath({{kFilters, kIndex}});
      msg = kFiltersValueInvalidMsg;
      break;
    case TriggerRegistrationError::kFiltersLookbackWindowValueInvalid:
      path = GetPath({{kFilters, FilterConfig::kLookbackWindowKey}});
      msg = kPositiveIntegerMsg;
      break;
    case TriggerRegistrationError::kFiltersUsingReservedKey:
      path = GetPath({{kFilters}});
      msg = kFiltersReservedKeysMsg;
      break;
    case TriggerRegistrationError::kFiltersListValueInvalid:
      path = GetPath({{kFilters, kIndex, kIndex}});
      msg = kFiltersValueInvalidMsg;
      break;
    case TriggerRegistrationError::kFiltersListLookbackWindowValueInvalid:
      path = GetPath({{kFilters, kIndex, FilterConfig::kLookbackWindowKey}});
      msg = kPositiveIntegerMsg;
      break;
    case TriggerRegistrationError::kFiltersListUsingReservedKey:
      path = GetPath({{kFilters, kIndex}});
      msg = kFiltersReservedKeysMsg;
      break;
    case TriggerRegistrationError::kAggregatableValuesWrongType:
      path = GetPath({{kAggregatableValues}});
      msg = kDictionaryOrListOfDictionariesMsg;
      break;
    case TriggerRegistrationError::kAggregatableValuesKeyTooLong:
      path = GetPath({{kAggregatableValues}});
      msg = AggregationKeyTooLongMsg();
      break;
    case TriggerRegistrationError::kAggregatableValuesListKeyTooLong:
      path = GetPath({{kAggregatableValues, kIndex, kValues}});
      msg = AggregationKeyTooLongMsg();
      break;
    case TriggerRegistrationError::kAggregatableValuesValueInvalid:
      path = GetPath({{kAggregatableValues, kIndex}});
      msg = AggregatableValueMsg();
      break;
    case TriggerRegistrationError::kAggregatableValuesListValueInvalid:
      path = GetPath({{kAggregatableValues, kIndex, kValues, kIndex}});
      msg = AggregatableValueMsg();
      break;
    case TriggerRegistrationError::kAggregatableValuesListValuesFieldMissing:
      path = GetPath({{kAggregatableValues, kIndex, kValues}});
      msg = kRequiredMsg;
      break;
    case TriggerRegistrationError::kAggregatableTriggerDataWrongType:
      path = GetPath({{kAggregatableTriggerData}});
      msg = kListOfDictionariesMsg;
      break;
    case TriggerRegistrationError::kAggregatableTriggerDataKeyPieceMissing:
      path = GetPath({{kAggregatableTriggerData, kIndex, kKeyPiece}});
      msg = kRequiredMsg;
      break;
    case TriggerRegistrationError::kAggregatableTriggerDataKeyPieceInvalid:
      path = GetPath({{kAggregatableTriggerData, kIndex, kKeyPiece}});
      msg = kAggregationKeyPieceMsg;
      break;
    case TriggerRegistrationError::kAggregatableTriggerDataSourceKeysInvalid:
      path = GetPath({{kAggregatableTriggerData, kIndex, kSourceKeys}});
      msg = base::StrCat(
          {"must be a list of strings, each with length less than or equal to ",
           base::NumberToString(kMaxBytesPerAggregationKeyId)});
      break;
    case TriggerRegistrationError::kEventTriggerDataWrongType:
      path = GetPath({{kEventTriggerData}});
      msg = kListOfDictionariesMsg;
      break;
    case TriggerRegistrationError::kEventTriggerDataValueInvalid:
      path = GetPath({{kEventTriggerData, kIndex, kTriggerData}});
      msg = base::StrCat({kBase10EncodedMsg, "uint64"});
      break;
    case TriggerRegistrationError::kEventPriorityValueInvalid:
      path = GetPath({{kEventTriggerData, kIndex, kPriority}});
      msg = base::StrCat({kBase10EncodedMsg, "int64"});
      break;
    case TriggerRegistrationError::kEventDedupKeyValueInvalid:
      path = GetPath({{kEventTriggerData, kIndex, kDeduplicationKey}});
      msg = base::StrCat({kBase10EncodedMsg, "uint64"});
      break;
    case TriggerRegistrationError::kAggregationCoordinatorValueInvalid:
      path = GetPath({{kAggregationCoordinatorOrigin}});
      msg =
          "must be a potentially trustworthy URL on the allowlist that uses "
          "HTTP/HTTPS";
      break;
    case TriggerRegistrationError::kAggregatableDedupKeyWrongType:
      path = GetPath({{kAggregatableDeduplicationKeys}});
      msg = kListOfDictionariesMsg;
      break;
    case TriggerRegistrationError::kAggregatableDedupKeyValueInvalid:
      path = GetPath(
          {{kAggregatableDeduplicationKeys, kIndex, kDeduplicationKey}});
      msg = base::StrCat({kBase10EncodedMsg, "uint64"});
      break;
    case TriggerRegistrationError::
        kAggregatableSourceRegistrationTimeValueInvalid:
      path = GetPath({{kAggregatableSourceRegistrationTime}});
      msg = EnumMsg(
          {{kSourceRegistrationTimeInclude, kSourceRegistrationTimeExclude}});
      break;
    case TriggerRegistrationError::kTriggerContextIdInvalidValue:
      path = GetPath({{kTriggerContextId}});
      msg = base::StrCat(
          {"must be a non-empty string with length less than or equal to ",
           base::NumberToString(kMaxTriggerContextIdLength)});
      break;
    case TriggerRegistrationError::
        kTriggerContextIdInvalidSourceRegistrationTimeConfig:
      path = GetPath({{kTriggerContextId}});
      msg = base::StrCat({"is prohibited for ",
                          kAggregatableSourceRegistrationTime, " ",
                          kSourceRegistrationTimeInclude});
      break;
    case TriggerRegistrationError::kEventValueInvalid:
      path = GetPath({{kEventTriggerData, kIndex, kValue}});
      msg = base::StrCat(
          {kPositiveIntegerMsg, " in the range [1, ",
           base::NumberToString(std::numeric_limits<uint32_t>::max()), "]"});
      break;
  }

  return SerializeErrorDetails(std::move(path), std::move(msg));
}

base::Value ErrorDetails(OsSourceRegistrationError error) {
  return ErrorDetails(*error);
}

base::Value ErrorDetails(OsTriggerRegistrationError error) {
  return ErrorDetails(*error);
}

}  // namespace

std::string_view RegistrationHeaderError::HeaderName() const {
  return absl::visit(base::Overloaded{
                         [](SourceRegistrationError) {
                           return kAttributionReportingRegisterSourceHeader;
                         },

                         [](TriggerRegistrationError) {
                           return kAttributionReportingRegisterTriggerHeader;
                         },

                         [](OsSourceRegistrationError) {
                           return kAttributionReportingRegisterOsSourceHeader;
                         },

                         [](OsTriggerRegistrationError) {
                           return kAttributionReportingRegisterOsTriggerHeader;
                         },
                     },
                     error_details);
}

base::Value RegistrationHeaderError::ErrorDetails() const {
  return absl::visit(
      [](auto error) { return attribution_reporting::ErrorDetails(error); },
      error_details);
}

}  // namespace attribution_reporting

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/popup_interaction.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/language/core/browser/language_usage_metrics.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill {

using mojom::SubmissionSource;

namespace {

using PaymentsRpcCardType =
    payments::PaymentsAutofillClient::PaymentsRpcCardType;
using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

constexpr char kUserActionRootPopupShown[] =
    "Autofill_PopupInteraction_PopupLevel_0_SuggestionShown";
constexpr char kUserActionSecondLevelPopupShown[] =
    "Autofill_PopupInteraction_PopupLevel_1_SuggestionShown";
constexpr char kUserActionThirdLevelPopupShown[] =
    "Autofill_PopupInteraction_PopupLevel_2_SuggestionShown";
constexpr char kUserActionRootPopupSuggestionSelected[] =
    "Autofill_PopupInteraction_PopupLevel_0_SuggestionSelected";
constexpr char kUserActionSecondLevelPopupSuggestionSelected[] =
    "Autofill_PopupInteraction_PopupLevel_1_SuggestionSelected";
constexpr char kUserActionThirdLevelPopupSuggestionSelected[] =
    "Autofill_PopupInteraction_PopupLevel_2_SuggestionSelected";
constexpr char kUserActionRootPopupSuggestionAccepted[] =
    "Autofill_PopupInteraction_PopupLevel_0_SuggestionAccepted";
constexpr char kUserActionSecondLevelPopupSuggestionAccepted[] =
    "Autofill_PopupInteraction_PopupLevel_1_SuggestionAccepted";
constexpr char kUserActionThirdLevelPopupSuggestionAccepted[] =
    "Autofill_PopupInteraction_PopupLevel_2_SuggestionAccepted";

// Exponential bucket spacing for UKM event data.
constexpr double kAutofillEventDataBucketSpacing = 2.0;

// Translates structured name types into simple names that are used for
// naming histograms.
constexpr auto kStructuredNameTypeToNameMap =
    base::MakeFixedFlatMap<FieldType, std::string_view>(
        {{NAME_FULL, "Full"},
         {NAME_FIRST, "First"},
         {NAME_MIDDLE, "Middle"},
         {NAME_LAST, "Last"},
         {NAME_LAST_FIRST, "FirstLast"},
         {NAME_LAST_SECOND, "SecondLast"}});

// Translates structured address types into simple names that are used for
// naming histograms.
constexpr auto kStructuredAddressTypeToNameMap =
    base::MakeFixedFlatMap<FieldType, std::string_view>(
        {{ADDRESS_HOME_STREET_ADDRESS, "StreetAddress"},
         {ADDRESS_HOME_STREET_NAME, "StreetName"},
         {ADDRESS_HOME_HOUSE_NUMBER, "HouseNumber"},
         {ADDRESS_HOME_FLOOR, "FloorNumber"},
         {ADDRESS_HOME_APT_NUM, "ApartmentNumber"},
         {ADDRESS_HOME_SUBPREMISE, "SubPremise"}});

// Don't change the enum values because they are recorded in metrics.
enum FieldTypeGroupForMetrics {
  GROUP_AMBIGUOUS = 0,
  GROUP_NAME = 1,
  GROUP_COMPANY = 2,
  GROUP_ADDRESS_LINE_1 = 3,
  GROUP_ADDRESS_LINE_2 = 4,
  GROUP_ADDRESS_CITY = 5,
  GROUP_ADDRESS_STATE = 6,
  GROUP_ADDRESS_ZIP = 7,
  GROUP_ADDRESS_COUNTRY = 8,
  GROUP_ADDRESS_HOME_STREET_NAME = 9,
  GROUP_ADDRESS_HOME_HOUSE_NUMBER = 10,
  GROUP_ADDRESS_HOME_SUBPREMISE = 11,
  GROUP_PHONE = 12,
  GROUP_FAX = 13,  // Deprecated.
  GROUP_EMAIL = 14,
  GROUP_CREDIT_CARD_NAME = 15,
  GROUP_CREDIT_CARD_NUMBER = 16,
  GROUP_CREDIT_CARD_DATE = 17,
  GROUP_CREDIT_CARD_TYPE = 18,
  GROUP_PASSWORD = 19,
  GROUP_ADDRESS_LINE_3 = 20,
  GROUP_USERNAME = 21,
  GROUP_STREET_ADDRESS = 22,
  GROUP_CREDIT_CARD_VERIFICATION = 23,
  GROUP_UNFILLABLE = 24,
  GROUP_ADDRESS_HOME_APT_NUM = 25,
  GROUP_ADDRESS_HOME_SORTING_CODE = 26,
  GROUP_ADDRESS_HOME_DEPENDENT_LOCALITY = 27,
  GROUP_ADDRESS_HOME_OTHER_SUBUNIT = 28,
  GROUP_ADDRESS_HOME_ADDRESS = 29,
  GROUP_ADDRESS_HOME_ADDRESS_WITH_NAME = 30,
  GROUP_ADDRESS_HOME_FLOOR = 31,
  GROUP_UNKNOWN_TYPE = 32,
  GROUP_BIRTHDATE = 33,  // Deprecated
  GROUP_IBAN = 34,
  GROUP_ADDRESS_HOME_LANDMARK = 35,
  GROUP_ADDRESS_HOME_BETWEEN_STREETS = 36,
  GROUP_ADDRESS_HOME_ADMIN_LEVEL2 = 37,
  GROUP_ADDRESS_HOME_STREET_LOCATION = 38,
  GROUP_ADDRESS_HOME_OVERFLOW = 39,
  GROUP_DELIVERY_INSTRUCTIONS = 40,
  GROUP_ADDRESS_HOME_OVERFLOW_AND_LANDMARK = 41,
  GROUP_ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK = 42,
  GROUP_ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY = 43,
  GROUP_ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK = 44,
  GROUP_ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK = 45,
  GROUP_ADDRESS_HOME_HOUSE_NUMBER_AND_APT = 46,
  GROUP_STANDALONE_CREDIT_CARD_VERIFICATION = 47,
  GROUP_PREDICTION_IMPROVEMENTS = 48,
  // Note: if adding an enum value here, run
  // tools/metrics/histograms/update_autofill_enums.py
  NUM_FIELD_TYPE_GROUPS_FOR_METRICS
};

// This defines a second-to-minute-scale prioritized set of buckets for
// recording user interaction time with forms. Pure exponential bucketing is
// generally not appropriate for analyzing interactions at this time scale, as
// we tend not to care about durations at the millisecond level, while small
// changes at the 2-3 minute scale may be invisible with exponential buckets.
// This set of buckets contains a large linear section between 1 and 30s, and
// between 30s and 10m, after which it proceeds in the same way as
// ukm::GetSemanticBucketMinForDurationTiming
int64_t GetSemanticBucketMinForAutofillDurationTiming(int64_t sample) {
  if (sample == 0) {
    return 0;
  }
  DCHECK(sample > 0);
  constexpr int64_t kMillisecondsPerSecond = 1000;
  constexpr int64_t kMillisecondsPerMinute = 60 * 1000;
  constexpr int64_t kMillisecondsPerHour = 60 * 60 * 1000;
  constexpr int64_t kMillisecondsPerDay = 24 * kMillisecondsPerHour;
  int64_t modulus;

  // If |sample| is a duration longer than a day, then use exponential bucketing
  // by number of days.
  // Algorithm is: convert ms to days, rounded down. Exponentially bucket.
  // Convert back to milliseconds, return sample.
  if (sample > kMillisecondsPerDay) {
    sample = sample / kMillisecondsPerDay;
    sample = ukm::GetExponentialBucketMinForUserTiming(sample);
    return sample * kMillisecondsPerDay;
  }

  if (sample > kMillisecondsPerHour) {
    // Above 1h, 1h granularity
    modulus = kMillisecondsPerHour;
  } else if (sample > 20 * kMillisecondsPerMinute) {
    // Above 20m, 10m granularity
    modulus = 10 * kMillisecondsPerMinute;
  } else if (sample > 10 * kMillisecondsPerMinute) {
    // Above 10m, 1m granularity
    modulus = kMillisecondsPerMinute;
  } else if (sample > 30 * kMillisecondsPerSecond) {
    // Above 30s, 5s granularity
    modulus = 5 * kMillisecondsPerSecond;
  } else {
    // Below 30s, 1s granularity
    modulus = kMillisecondsPerSecond;
  }
  return sample - (sample % modulus);
}

}  // namespace

// First, translates |field_type| to the corresponding logical |group| from
// |FieldTypeGroupForMetrics|.  Then, interpolates this with the given |metric|,
// which should be in the range [0, |num_possible_metrics|).
// Returns the interpolated index.
//
// The interpolation maps the pair (|group|, |metric|) to a single index, so
// that all the indices for a given group are adjacent.  In particular, with
// the groups {AMBIGUOUS, NAME, ...} combining with the metrics
// {UNKNOWN, MATCH, MISMATCH}, we create this set of mapped indices:
// {
//   AMBIGUOUS+UNKNOWN,
//   AMBIGUOUS+MATCH,
//   AMBIGUOUS+MISMATCH,
//   NAME+UNKNOWN,
//   NAME+MATCH,
//   NAME+MISMATCH,
//   ...
// }.
//
// Clients must ensure that |field_type| is one of the types Chrome supports
// natively, e.g. |field_type| must not be a billing address.
// NOTE: This is defined outside of the anonymous namespace so that it can be
// accessed from the unit test file. It is not exposed in the header file,
// however, because it is not intended for consumption outside of the metrics
// implementation.
int GetFieldTypeGroupPredictionQualityMetric(
    FieldType field_type,
    AutofillMetrics::FieldTypeQualityMetric metric) {
  DCHECK_LT(metric, AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);

  FieldTypeGroupForMetrics group = GROUP_AMBIGUOUS;
  switch (GroupTypeOfFieldType(field_type)) {
    case FieldTypeGroup::kNoGroup:
      group = GROUP_AMBIGUOUS;
      break;

    case FieldTypeGroup::kName:
      group = GROUP_NAME;
      break;

    case FieldTypeGroup::kCompany:
      group = GROUP_COMPANY;
      break;

    case FieldTypeGroup::kIban:
      group = GROUP_IBAN;
      break;

    case FieldTypeGroup::kPredictionImprovements:
      group = GROUP_PREDICTION_IMPROVEMENTS;
      break;

    case FieldTypeGroup::kAddress:
      switch (field_type) {
        case ADDRESS_HOME_LINE1:
          group = GROUP_ADDRESS_LINE_1;
          break;
        case ADDRESS_HOME_LINE2:
          group = GROUP_ADDRESS_LINE_2;
          break;
        case ADDRESS_HOME_LINE3:
          group = GROUP_ADDRESS_LINE_3;
          break;
        case ADDRESS_HOME_APT:
        case ADDRESS_HOME_APT_NUM:
        case ADDRESS_HOME_APT_TYPE:
          group = GROUP_ADDRESS_HOME_APT_NUM;
          break;
        case ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
          group = GROUP_ADDRESS_HOME_HOUSE_NUMBER_AND_APT;
          break;
        case ADDRESS_HOME_STREET_ADDRESS:
          group = GROUP_STREET_ADDRESS;
          break;
        case ADDRESS_HOME_CITY:
          group = GROUP_ADDRESS_CITY;
          break;
        case ADDRESS_HOME_STATE:
          group = GROUP_ADDRESS_STATE;
          break;
        case ADDRESS_HOME_ZIP:
          group = GROUP_ADDRESS_ZIP;
          break;
        case ADDRESS_HOME_COUNTRY:
          group = GROUP_ADDRESS_COUNTRY;
          break;
        case ADDRESS_HOME_STREET_NAME:
          group = GROUP_ADDRESS_HOME_STREET_NAME;
          break;
        case ADDRESS_HOME_SORTING_CODE:
          group = GROUP_ADDRESS_HOME_SORTING_CODE;
          break;
        case ADDRESS_HOME_DEPENDENT_LOCALITY:
          group = GROUP_ADDRESS_HOME_DEPENDENT_LOCALITY;
          break;
        case ADDRESS_HOME_HOUSE_NUMBER:
          group = GROUP_ADDRESS_HOME_HOUSE_NUMBER;
          break;
        case ADDRESS_HOME_SUBPREMISE:
          group = GROUP_ADDRESS_HOME_SUBPREMISE;
          break;
        case ADDRESS_HOME_OTHER_SUBUNIT:
          group = GROUP_ADDRESS_HOME_OTHER_SUBUNIT;
          break;
        case ADDRESS_HOME_ADDRESS:
          group = GROUP_ADDRESS_HOME_ADDRESS;
          break;
        case ADDRESS_HOME_ADDRESS_WITH_NAME:
          group = GROUP_ADDRESS_HOME_ADDRESS_WITH_NAME;
          break;
        case ADDRESS_HOME_FLOOR:
          group = GROUP_ADDRESS_HOME_FLOOR;
          break;
        case ADDRESS_HOME_LANDMARK:
          group = GROUP_ADDRESS_HOME_LANDMARK;
          break;
        case ADDRESS_HOME_BETWEEN_STREETS:
        case ADDRESS_HOME_BETWEEN_STREETS_1:
        case ADDRESS_HOME_BETWEEN_STREETS_2:
          group = GROUP_ADDRESS_HOME_BETWEEN_STREETS;
          break;
        case ADDRESS_HOME_ADMIN_LEVEL2:
          group = GROUP_ADDRESS_HOME_ADMIN_LEVEL2;
          break;
        case ADDRESS_HOME_OVERFLOW:
          group = GROUP_ADDRESS_HOME_OVERFLOW;
          break;
        case ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
          group = GROUP_ADDRESS_HOME_OVERFLOW_AND_LANDMARK;
          break;
        case ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
          group = GROUP_ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK;
          break;
        case ADDRESS_HOME_STREET_LOCATION:
          group = GROUP_ADDRESS_HOME_STREET_LOCATION;
          break;
        case ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
          group = GROUP_ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY;
          break;
        case ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
          group = GROUP_ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK;
          break;
        case ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
          group = GROUP_ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK;
          break;
        case DELIVERY_INSTRUCTIONS:
          group = GROUP_DELIVERY_INSTRUCTIONS;
          break;
        case UNKNOWN_TYPE:
          group = GROUP_UNKNOWN_TYPE;
          break;
        case NO_SERVER_DATA:
        case EMPTY_TYPE:
        case NAME_FIRST:
        case NAME_MIDDLE:
        case NAME_LAST:
        case NAME_MIDDLE_INITIAL:
        case NAME_FULL:
        case NAME_SUFFIX:
        case EMAIL_ADDRESS:
        case PHONE_HOME_NUMBER:
        case PHONE_HOME_NUMBER_PREFIX:
        case PHONE_HOME_NUMBER_SUFFIX:
        case PHONE_HOME_CITY_CODE:
        case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
        case PHONE_HOME_COUNTRY_CODE:
        case PHONE_HOME_CITY_AND_NUMBER:
        case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
        case PHONE_HOME_WHOLE_NUMBER:
        case CREDIT_CARD_NAME_FULL:
        case CREDIT_CARD_NUMBER:
        case CREDIT_CARD_EXP_MONTH:
        case CREDIT_CARD_EXP_2_DIGIT_YEAR:
        case CREDIT_CARD_EXP_4_DIGIT_YEAR:
        case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
        case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
        case CREDIT_CARD_TYPE:
        case CREDIT_CARD_VERIFICATION_CODE:
        case COMPANY_NAME:
        case FIELD_WITH_DEFAULT_VALUE:
        case MERCHANT_EMAIL_SIGNUP:
        case MERCHANT_PROMO_CODE:
        case PASSWORD:
        case ACCOUNT_CREATION_PASSWORD:
        case NOT_ACCOUNT_CREATION_PASSWORD:
        case USERNAME:
        case USERNAME_AND_EMAIL_ADDRESS:
        case NEW_PASSWORD:
        case PROBABLY_NEW_PASSWORD:
        case NOT_NEW_PASSWORD:
        case CREDIT_CARD_NAME_FIRST:
        case CREDIT_CARD_NAME_LAST:
        case PHONE_HOME_EXTENSION:
        case CONFIRMATION_PASSWORD:
        case AMBIGUOUS_TYPE:
        case SEARCH_TERM:
        case PRICE:
        case NUMERIC_QUANTITY:
        case NOT_PASSWORD:
        case SINGLE_USERNAME:
        case NOT_USERNAME:
        case ONE_TIME_CODE:
        case NAME_LAST_FIRST:
        case NAME_LAST_CONJUNCTION:
        case NAME_LAST_SECOND:
        case NAME_HONORIFIC_PREFIX:
        case IBAN_VALUE:
        case MAX_VALID_FIELD_TYPE:
        case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
        case SINGLE_USERNAME_FORGOT_PASSWORD:
        case SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
        case IMPROVED_PREDICTION:
          NOTREACHED_IN_MIGRATION()
              << field_type << " type is not in that group.";
          group = GROUP_AMBIGUOUS;
          break;
      }
      break;

    case FieldTypeGroup::kEmail:
      group = GROUP_EMAIL;
      break;

    case FieldTypeGroup::kPhone:
      group = GROUP_PHONE;
      break;

    case FieldTypeGroup::kCreditCard:
      switch (field_type) {
        case CREDIT_CARD_NAME_FULL:
        case CREDIT_CARD_NAME_FIRST:
        case CREDIT_CARD_NAME_LAST:
          group = GROUP_CREDIT_CARD_NAME;
          break;
        case CREDIT_CARD_NUMBER:
          group = GROUP_CREDIT_CARD_NUMBER;
          break;
        case CREDIT_CARD_TYPE:
          group = GROUP_CREDIT_CARD_TYPE;
          break;
        case CREDIT_CARD_EXP_MONTH:
        case CREDIT_CARD_EXP_2_DIGIT_YEAR:
        case CREDIT_CARD_EXP_4_DIGIT_YEAR:
        case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
        case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
          group = GROUP_CREDIT_CARD_DATE;
          break;
        case CREDIT_CARD_VERIFICATION_CODE:
          group = GROUP_CREDIT_CARD_VERIFICATION;
          break;
        default:
          NOTREACHED_IN_MIGRATION()
              << field_type << " has no group assigned (ambiguous)";
          group = GROUP_AMBIGUOUS;
          break;
      }
      break;

    case FieldTypeGroup::kStandaloneCvcField:
      group = GROUP_STANDALONE_CREDIT_CARD_VERIFICATION;
      break;

    case FieldTypeGroup::kPasswordField:
      group = GROUP_PASSWORD;
      break;

    case FieldTypeGroup::kUsernameField:
      group = GROUP_USERNAME;
      break;

    case FieldTypeGroup::kUnfillable:
      group = GROUP_UNFILLABLE;
      break;

    case FieldTypeGroup::kTransaction:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // Use bits 8-15 for the group and bits 0-7 for the metric.
  static_assert(AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS <= UINT8_MAX,
                "maximum field type quality metric must fit into 8 bits");
  static_assert(NUM_FIELD_TYPE_GROUPS_FOR_METRICS <= UINT8_MAX,
                "number of field type groups must fit into 8 bits");
  return (group << 8) | metric;
}

// This function encodes the integer value of a |FieldType| and the
// metric value of an |AutofilledFieldUserEditingStatus| into a 16 bit integer.
// The lower four bits are used to encode the editing status and the higher
// 12 bits are used to encode the field type.
int GetFieldTypeUserEditStatusMetric(
    FieldType server_type,
    AutofillMetrics::AutofilledFieldUserEditingStatusMetric metric) {
  static_assert(FieldType::MAX_VALID_FIELD_TYPE <= (UINT16_MAX >> 4),
                "Autofill::ServerTypes value needs more than 12 bits.");

  static_assert(
      static_cast<int>(
          AutofillMetrics::AutofilledFieldUserEditingStatusMetric::kMaxValue) <=
          (UINT16_MAX >> 12),
      "AutofillMetrics::AutofilledFieldUserEditingStatusMetric value needs "
      "more than 4 bits");

  return (server_type << 4) | static_cast<int>(metric);
}

namespace {

const char* GetQualityMetricPredictionSource(
    AutofillMetrics::QualityMetricPredictionSource source) {
  switch (source) {
    case AutofillMetrics::PREDICTION_SOURCE_UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      return "Unknown";

    case AutofillMetrics::PREDICTION_SOURCE_HEURISTIC:
      return "Heuristic";
    case AutofillMetrics::PREDICTION_SOURCE_SERVER:
      return "Server";
    case AutofillMetrics::PREDICTION_SOURCE_OVERALL:
      return "Overall";
    case AutofillMetrics::PREDICTION_SOURCE_ML_PREDICTIONS:
      return "ML";
  }
}

const char* GetQualityMetricTypeSuffix(
    AutofillMetrics::QualityMetricType metric_type) {
  switch (metric_type) {
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case AutofillMetrics::TYPE_SUBMISSION:
      return "";
    case AutofillMetrics::TYPE_NO_SUBMISSION:
      return ".NoSubmission";
    case AutofillMetrics::TYPE_AUTOCOMPLETE_BASED:
      return ".BasedOnAutocomplete";
  }
}

// Given a set of |possible_types| for a field, select the best type to use as
// the "actual" field type when calculating metrics. If the |predicted_type| is
// among the |possible_types] then use that as the best type (i.e., the
// prediction is deemed to have been correct).
FieldType GetActualFieldType(const FieldTypeSet& possible_types,
                             FieldType predicted_type) {
  DCHECK_NE(possible_types.size(), 0u);

  if (possible_types.count(EMPTY_TYPE)) {
    DCHECK_EQ(possible_types.size(), 1u);
    return EMPTY_TYPE;
  }

  if (possible_types.count(UNKNOWN_TYPE)) {
    DCHECK_EQ(possible_types.size(), 1u);
    return UNKNOWN_TYPE;
  }

  if (possible_types.count(predicted_type))
    return predicted_type;

  // Collapse field types that Chrome treats as identical, e.g. home and
  // billing address fields.
  FieldTypeSet collapsed_field_types;
  for (FieldType type : possible_types) {
    DCHECK_NE(type, EMPTY_TYPE);
    DCHECK_NE(type, UNKNOWN_TYPE);

    // A phone number that's only missing its country code is (for metrics
    // purposes) the same as the whole phone number.
    if (type == PHONE_HOME_CITY_AND_NUMBER) {
      collapsed_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
    } else {
      collapsed_field_types.insert(type);
    }
  }

  // Capture the field's type, if it is unambiguous.
  FieldType actual_type = AMBIGUOUS_TYPE;
  if (collapsed_field_types.size() == 1)
    actual_type = *collapsed_field_types.begin();

  DVLOG(2) << "Inferred Type: " << FieldTypeToStringView(actual_type);
  return actual_type;
}

// Check if the value of |field| is same as one of the other autofilled
// values. This indicates a bad rationalization if |field| has
// only_fill_when_focused set to true.
bool DuplicatedFilling(const FormStructure& form, const AutofillField& field) {
  auto is_autofilled_with_same_value =
      [&field](const std::unique_ptr<AutofillField>& form_field) {
        if (field.global_id() == form_field->global_id()) {
          // When looking for fields in the form that have been filled with
          // the same value as `field`, skip `field`: `field` would
          // always have a matching value but does not indicate that a
          // *duplicate* filling happened.
          return false;
        }
        return field.value_for_import() == form_field->value_for_import() &&
               form_field->is_autofilled();
      };
  return std::ranges::any_of(form, is_autofilled_with_same_value);
}

void LogOnlyFillWhenFocusedRationalizationQuality(
    const std::string& aggregate_histogram,
    const std::string& type_specific_histogram,
    FieldType predicted_type,
    FieldType actual_type,
    bool is_empty,
    bool is_ambiguous,
    bool log_rationalization_metrics,
    const FormStructure& form,
    const AutofillField& field) {
  static constexpr char kRationalizationQualityHistogram[] =
      "Autofill.Rationalization.OnlyFillWhenFocused.Quality";
  // If it is filled with values unknown, it is a true negative.
  if (actual_type == UNKNOWN_TYPE) {
    // Only log aggregate true negative; do not log type specific metrics
    // for UNKNOWN/EMPTY.
    DVLOG(2) << "TRUE NEGATIVE";
    base::UmaHistogramSparse(
        aggregate_histogram,
        (is_empty ? AutofillMetrics::TRUE_NEGATIVE_EMPTY
                  : (is_ambiguous ? AutofillMetrics::TRUE_NEGATIVE_AMBIGUOUS
                                  : AutofillMetrics::TRUE_NEGATIVE_UNKNOWN)));
    if (log_rationalization_metrics) {
      base::UmaHistogramSparse(
          kRationalizationQualityHistogram,
          (is_empty ? AutofillMetrics::RATIONALIZATION_GOOD
                    : AutofillMetrics::RATIONALIZATION_OK));
    }
    return;
  }

  // If it is filled with same type as predicted, it is a true positive. We
  // also log an RATIONALIZATION_BAD by checking if the filled value is filled
  // already in previous fields, this means autofill could have filled it
  // automatically if there has been no rationalization.
  if (predicted_type == actual_type) {
    DVLOG(2) << "TRUE POSITIVE";
    base::UmaHistogramSparse(aggregate_histogram,
                             AutofillMetrics::TRUE_POSITIVE);
    base::UmaHistogramSparse(type_specific_histogram,
                             GetFieldTypeGroupPredictionQualityMetric(
                                 actual_type, AutofillMetrics::TRUE_POSITIVE));
    if (log_rationalization_metrics) {
      bool duplicated_filling = DuplicatedFilling(form, field);
      base::UmaHistogramSparse(
          kRationalizationQualityHistogram,
          (duplicated_filling ? AutofillMetrics::RATIONALIZATION_BAD
                              : AutofillMetrics::RATIONALIZATION_OK));
    }
    return;
  }

  DVLOG(2) << "MISMATCH";
  // Here the prediction is wrong, but user has to provide some value still.
  // This should be a false negative.
  base::UmaHistogramSparse(aggregate_histogram,
                           AutofillMetrics::FALSE_NEGATIVE_MISMATCH);
  // Log FALSE_NEGATIVE_MISMATCH for predicted type if it did predicted
  // something but actual type is different.
  if (predicted_type != UNKNOWN_TYPE)
    base::UmaHistogramSparse(
        type_specific_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            predicted_type, AutofillMetrics::FALSE_NEGATIVE_MISMATCH));
  if (log_rationalization_metrics) {
    // Logging RATIONALIZATION_OK despite of type mismatch here because autofill
    // would have got it wrong with or without rationalization. Rationalization
    // here does not help, neither does it do any harm.
    base::UmaHistogramSparse(kRationalizationQualityHistogram,
                             AutofillMetrics::RATIONALIZATION_OK);
  }
  return;
}

void LogPredictionQualityMetricsForCommonFields(
    const std::string& aggregate_histogram,
    const std::string& type_specific_histogram,
    FieldType predicted_type,
    FieldType actual_type,
    bool is_empty,
    bool is_ambiguous) {
  // If the predicted and actual types match then it's either a true positive
  // or a true negative (if they are both unknown). Do not log type specific
  // true negatives (instead log a true positive for the "Ambiguous" type).
  if (predicted_type == actual_type) {
    if (actual_type == UNKNOWN_TYPE) {
      // Only log aggregate true negative; do not log type specific metrics
      // for UNKNOWN/EMPTY.
      DVLOG(2) << "TRUE NEGATIVE";
      base::UmaHistogramSparse(
          aggregate_histogram,
          (is_empty ? AutofillMetrics::TRUE_NEGATIVE_EMPTY
                    : (is_ambiguous ? AutofillMetrics::TRUE_NEGATIVE_AMBIGUOUS
                                    : AutofillMetrics::TRUE_NEGATIVE_UNKNOWN)));
      return;
    }

    DVLOG(2) << "TRUE POSITIVE";
    // Log both aggregate and type specific true positive if we correctly
    // predict that type with which the field was filled.
    base::UmaHistogramSparse(aggregate_histogram,
                             AutofillMetrics::TRUE_POSITIVE);
    base::UmaHistogramSparse(type_specific_histogram,
                             GetFieldTypeGroupPredictionQualityMetric(
                                 actual_type, AutofillMetrics::TRUE_POSITIVE));
    return;
  }

  // Note: At this point predicted_type != actual type
  // If actual type is UNKNOWN_TYPE then the prediction is a false positive.
  // Further specialize the type of false positive by whether the field was
  // empty or contained an unknown value.
  if (actual_type == UNKNOWN_TYPE) {
    DVLOG(2) << "FALSE POSITIVE";
    auto metric =
        (is_empty ? AutofillMetrics::FALSE_POSITIVE_EMPTY
                  : (is_ambiguous ? AutofillMetrics::FALSE_POSITIVE_AMBIGUOUS
                                  : AutofillMetrics::FALSE_POSITIVE_UNKNOWN));
    base::UmaHistogramSparse(aggregate_histogram, metric);
    base::UmaHistogramSparse(
        type_specific_histogram,
        GetFieldTypeGroupPredictionQualityMetric(predicted_type, metric));
    return;
  }

  // Note: At this point predicted_type != actual type, actual_type != UNKNOWN.
  // If predicted type is UNKNOWN_TYPE then the prediction is a false negative
  // unknown.
  if (predicted_type == UNKNOWN_TYPE) {
    DVLOG(2) << "FALSE NEGATIVE";
    base::UmaHistogramSparse(aggregate_histogram,
                             AutofillMetrics::FALSE_NEGATIVE_UNKNOWN);
    base::UmaHistogramSparse(
        type_specific_histogram,
        GetFieldTypeGroupPredictionQualityMetric(
            actual_type, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN));
    return;
  }

  DVLOG(2) << "MISMATCH";

  // Note: At this point predicted_type != actual type, actual_type != UNKNOWN,
  //       predicted_type != UNKNOWN.
  // This is a mismatch. From the reference of the actual type, this is a false
  // negative (it was T, but predicted U). From the reference of the prediction,
  // this is a false positive (predicted it was T, but it was U).
  base::UmaHistogramSparse(aggregate_histogram,
                           AutofillMetrics::FALSE_NEGATIVE_MISMATCH);
  base::UmaHistogramSparse(
      type_specific_histogram,
      GetFieldTypeGroupPredictionQualityMetric(
          actual_type, AutofillMetrics::FALSE_NEGATIVE_MISMATCH));
  base::UmaHistogramSparse(
      type_specific_histogram,
      GetFieldTypeGroupPredictionQualityMetric(
          predicted_type, AutofillMetrics::FALSE_POSITIVE_MISMATCH));
}

// Logs field type prediction quality metrics.  The primary histogram name is
// constructed based on |prediction_source| The field-specific histogram names
// also incorporates the possible and predicted types for |field|. A suffix may
// be appended to the metric name, depending on |metric_type|.
void LogPredictionQualityMetrics(
    AutofillMetrics::QualityMetricPredictionSource prediction_source,
    FieldType predicted_type,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    AutofillMetrics::QualityMetricType metric_type,
    bool log_rationalization_metrics) {
  // Generate histogram names.
  const char* source = GetQualityMetricPredictionSource(prediction_source);
  const char* suffix = GetQualityMetricTypeSuffix(metric_type);
  std::string raw_data_histogram =
      base::JoinString({"Autofill.FieldPrediction.", source, suffix}, "");
  std::string aggregate_histogram = base::JoinString(
      {"Autofill.FieldPredictionQuality.Aggregate.", source, suffix}, "");
  std::string type_specific_histogram = base::JoinString(
      {"Autofill.FieldPredictionQuality.ByFieldType.", source, suffix}, "");

  const FieldTypeSet& possible_types =
      metric_type == AutofillMetrics::TYPE_AUTOCOMPLETE_BASED
          ? FieldTypeSet{AutofillType(field.html_type()).GetStorableType()}
          : field.possible_types();

  // Get the best type classification we can for the field.
  FieldType actual_type = GetActualFieldType(possible_types, predicted_type);

  DVLOG(2) << "Predicted: " << FieldTypeToStringView(predicted_type) << "; "
           << "Actual: " << FieldTypeToStringView(actual_type);

  DCHECK_LE(predicted_type, UINT16_MAX);
  DCHECK_LE(actual_type, UINT16_MAX);
  base::UmaHistogramSparse(raw_data_histogram,
                           (predicted_type << 16) | actual_type);

  form_interactions_ukm_logger->LogFieldType(
      form.form_parsed_timestamp(), form.form_signature(),
      field.GetFieldSignature(), prediction_source, metric_type, predicted_type,
      actual_type);

  // NO_SERVER_DATA is the equivalent of predicting UNKNOWN.
  if (predicted_type == NO_SERVER_DATA)
    predicted_type = UNKNOWN_TYPE;

  // The actual type being EMPTY_TYPE is the same as UNKNOWN_TYPE for comparison
  // purposes, but remember whether or not it was empty for more precise logging
  // later.
  bool is_empty = (actual_type == EMPTY_TYPE);
  bool is_ambiguous = (actual_type == AMBIGUOUS_TYPE);
  if (is_empty || is_ambiguous)
    actual_type = UNKNOWN_TYPE;

  // Log metrics for a field that is |only_fill_when_focused|==true. Basically
  // autofill might have a field prediction but it also thinks it should not
  // be filled automatically unless user focused on the field. This requires
  // different metrics logging than normal fields.
  if (field.only_fill_when_focused()) {
    LogOnlyFillWhenFocusedRationalizationQuality(
        aggregate_histogram, type_specific_histogram, predicted_type,
        actual_type, is_empty, is_ambiguous, log_rationalization_metrics, form,
        field);
    return;
  }

  LogPredictionQualityMetricsForCommonFields(
      aggregate_histogram, type_specific_histogram, predicted_type, actual_type,
      is_empty, is_ambiguous);
}

}  // namespace

const int kMaxBucketsCount = 50;

// static
AutofillMetrics::AutocompleteState
AutofillMetrics::AutocompleteStateForSubmittedField(
    const AutofillField& field) {
  // An unparsable autocomplete attribute is treated like kNone.
  auto autocomplete_state = AutofillMetrics::AutocompleteState::kNone;
  // autocomplete=on is ignored as well. But for the purposes of metrics we care
  // about cases where the developer tries to disable autocomplete.
  if (field.autocomplete_attribute() != "on" &&
      ShouldIgnoreAutocompleteAttribute(field.autocomplete_attribute())) {
    autocomplete_state = AutofillMetrics::AutocompleteState::kOff;
  } else if (field.parsed_autocomplete()) {
    autocomplete_state =
        field.parsed_autocomplete()->field_type != HtmlFieldType::kUnrecognized
            ? AutofillMetrics::AutocompleteState::kValid
            : AutofillMetrics::AutocompleteState::kGarbage;

    if (field.autocomplete_attribute() == "new-password" ||
        field.autocomplete_attribute() == "current-password") {
      autocomplete_state = AutofillMetrics::AutocompleteState::kPassword;
    }
  }

  return autocomplete_state;
}

// static
void AutofillMetrics::LogSubmittedCardStateMetric(
    SubmittedCardStateMetric metric) {
  DCHECK_LT(metric, NUM_SUBMITTED_CARD_STATE_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.SubmittedCardState", metric,
                            NUM_SUBMITTED_CARD_STATE_METRICS);
}

// static
void AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
    SubmittedServerCardExpirationStatusMetric metric) {
  DCHECK_LT(metric, NUM_SUBMITTED_SERVER_CARD_EXPIRATION_STATUS_METRICS);
  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.SubmittedServerCardExpirationStatus", metric,
      NUM_SUBMITTED_SERVER_CARD_EXPIRATION_STATUS_METRICS);
}

// static
void AutofillMetrics::LogUploadOfferedCardOriginMetric(
    UploadOfferedCardOriginMetric metric) {
  DCHECK_LT(metric, NUM_UPLOAD_OFFERED_CARD_ORIGIN_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.UploadOfferedCardOrigin", metric,
                            NUM_UPLOAD_OFFERED_CARD_ORIGIN_METRICS);
}

// static
void AutofillMetrics::LogUploadAcceptedCardOriginMetric(
    UploadAcceptedCardOriginMetric metric) {
  DCHECK_LT(metric, NUM_UPLOAD_ACCEPTED_CARD_ORIGIN_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.UploadAcceptedCardOrigin", metric,
                            NUM_UPLOAD_ACCEPTED_CARD_ORIGIN_METRICS);
}

// static
void AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogResultMetric(
    CardUnmaskAuthenticationSelectionDialogResultMetric metric) {
  DCHECK_LE(metric,
            CardUnmaskAuthenticationSelectionDialogResultMetric::kMaxValue);
  base::UmaHistogramEnumeration(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result", metric);
}

// static
void AutofillMetrics::LogCardUnmaskAuthenticationSelectionDialogShown(
    size_t number_of_challenge_options) {
  static_assert(static_cast<int>(CardUnmaskChallengeOptionType::kMaxValue) <
                10);
  DCHECK_GE(number_of_challenge_options, 0U);
  // We are using an exact linear histogram, with a max of 10. This is a
  // reasonable max so that the histogram is not sparse, as we do not foresee
  // ever having more than 10 challenge options at the same time on a dialog to
  // authenticate a virtual card.
  base::UmaHistogramExactLinear(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Shown2",
      number_of_challenge_options, /*exclusive_max=*/10);
}

// static
void AutofillMetrics::LogCreditCardInfoBarMetric(
    InfoBarMetric metric,
    bool is_uploading,
    payments::PaymentsAutofillClient::SaveCreditCardOptions options) {
  DCHECK_LT(metric, NUM_INFO_BAR_METRICS);

  std::string destination = is_uploading ? ".Server" : ".Local";
  base::UmaHistogramEnumeration("Autofill.CreditCardInfoBar" + destination,
                                metric, NUM_INFO_BAR_METRICS);
  if (options.should_request_name_from_user) {
    base::UmaHistogramEnumeration("Autofill.CreditCardInfoBar" + destination +
                                      ".RequestingCardholderName",
                                  metric, NUM_INFO_BAR_METRICS);
  }

  if (options.should_request_expiration_date_from_user) {
    base::UmaHistogramEnumeration("Autofill.CreditCardInfoBar" + destination +
                                      ".RequestingExpirationDate",
                                  metric, NUM_INFO_BAR_METRICS);
  }

  if (options.has_multiple_legal_lines) {
    base::UmaHistogramEnumeration(
        "Autofill.CreditCardInfoBar" + destination + ".WithMultipleLegalLines",
        metric, NUM_INFO_BAR_METRICS);
  }

  if (options.has_same_last_four_as_server_card_but_different_expiration_date) {
    base::UmaHistogramEnumeration("Autofill.CreditCardInfoBar" + destination +
                                      ".WithSameLastFourButDifferentExpiration",
                                  metric, NUM_INFO_BAR_METRICS);
  }

  if (options.card_save_type ==
      payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc) {
    base::UmaHistogramEnumeration(base::StrCat({"Autofill.CreditCardInfoBar",
                                                destination, ".SavingWithCvc"}),
                                  metric, NUM_INFO_BAR_METRICS);
  }
}

// static
void AutofillMetrics::LogScanCreditCardPromptMetric(
    ScanCreditCardPromptMetric metric) {
  DCHECK_LT(metric, NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.ScanCreditCardPrompt", metric,
                            NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
}

// static
void AutofillMetrics::LogScanCreditCardCompleted(base::TimeDelta duration,
                                                 bool completed) {
  std::string suffix = completed ? "Completed" : "Cancelled";
  base::UmaHistogramLongTimes("Autofill.ScanCreditCard.Duration_" + suffix,
                              duration);
  UMA_HISTOGRAM_BOOLEAN("Autofill.ScanCreditCard.Completed", completed);
}

// static
void AutofillMetrics::LogProgressDialogResultMetric(
    bool is_canceled_by_user,
    AutofillProgressDialogType autofill_progress_dialog_type) {
  base::UmaHistogramBoolean(base::StrCat({"Autofill.ProgressDialog.",
                                          GetDialogTypeStringForLogging(
                                              autofill_progress_dialog_type),
                                          ".Result"}),
                            is_canceled_by_user);
}

void AutofillMetrics::LogProgressDialogShown(
    AutofillProgressDialogType autofill_progress_dialog_type) {
  base::UmaHistogramBoolean(base::StrCat({"Autofill.ProgressDialog.",
                                          GetDialogTypeStringForLogging(
                                              autofill_progress_dialog_type),
                                          ".Shown"}),
                            true);
}

std::string_view AutofillMetrics::GetDialogTypeStringForLogging(
    AutofillProgressDialogType autofill_progress_dialog_type) {
  switch (autofill_progress_dialog_type) {
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
      return "VirtualCardUnmask";
    case AutofillProgressDialogType::kServerCardUnmaskProgressDialog:
      return "ServerCardUnmask";
    case AutofillProgressDialogType::kServerIbanUnmaskProgressDialog:
      return "ServerIbanUnmask";
    case AutofillProgressDialogType::k3dsFetchVcnProgressDialog:
      return "3dsFetchVirtualCard";
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
  }
}

// static
void AutofillMetrics::LogUnmaskPromptEvent(UnmaskPromptEvent event,
                                           bool has_valid_nickname,
                                           CreditCard::RecordType card_type) {
  base::UmaHistogramEnumeration("Autofill.UnmaskPrompt" +
                                    GetHistogramStringForCardType(card_type) +
                                    ".Events",
                                event, NUM_UNMASK_PROMPT_EVENTS);
  if (has_valid_nickname) {
    base::UmaHistogramEnumeration("Autofill.UnmaskPrompt.Events.WithNickname",
                                  event, NUM_UNMASK_PROMPT_EVENTS);
  }
}

// static
void AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
    CardholderNameFixFlowPromptEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.CardholderNameFixFlowPrompt.Events",
                            event, NUM_CARDHOLDER_NAME_FIXFLOW_PROMPT_EVENTS);
}

// static
void AutofillMetrics::LogExpirationDateFixFlowPromptEvent(
    ExpirationDateFixFlowPromptEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.ExpirationDateFixFlowPrompt.Events",
                            event);
}

// static
void AutofillMetrics::LogExpirationDateFixFlowPromptShown() {
  UMA_HISTOGRAM_BOOLEAN("Autofill.ExpirationDateFixFlowPromptShown", true);
}

// static
void AutofillMetrics::LogUnmaskPromptEventDuration(
    base::TimeDelta duration,
    UnmaskPromptEvent close_event,
    bool has_valid_nickname) {
  std::string suffix;
  switch (close_event) {
    case UNMASK_PROMPT_CLOSED_NO_ATTEMPTS:
      suffix = ".NoAttempts";
      break;
    case UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE:
    case UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE:
      suffix = ".Failure";
      break;
    case UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING:
      suffix = ".AbandonUnmasking";
      break;
    case UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT:
    case UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS:
      suffix = ".Success";
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.Duration", duration);
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.Duration" + suffix,
                              duration);

  if (has_valid_nickname) {
    base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.Duration.WithNickname",
                                duration);
    base::UmaHistogramLongTimes(
        "Autofill.UnmaskPrompt.Duration" + suffix + ".WithNickname", duration);
  }
}

// static
void AutofillMetrics::LogTimeBeforeAbandonUnmasking(base::TimeDelta duration,
                                                    bool has_valid_nickname) {
  base::UmaHistogramLongTimes(
      "Autofill.UnmaskPrompt.TimeBeforeAbandonUnmasking", duration);
  if (has_valid_nickname) {
    base::UmaHistogramLongTimes(
        "Autofill.UnmaskPrompt.TimeBeforeAbandonUnmasking.WithNickname",
        duration);
  }
}

// static
void AutofillMetrics::LogRealPanResult(PaymentsRpcResult result,
                                       PaymentsRpcCardType card_type) {
  PaymentsRpcMetricResult metric_result;
  switch (result) {
    case PaymentsRpcResult::kSuccess:
      metric_result = PAYMENTS_RESULT_SUCCESS;
      break;
    case PaymentsRpcResult::kTryAgainFailure:
      metric_result = PAYMENTS_RESULT_TRY_AGAIN_FAILURE;
      break;
    case PaymentsRpcResult::kPermanentFailure:
      metric_result = PAYMENTS_RESULT_PERMANENT_FAILURE;
      break;
    case PaymentsRpcResult::kNetworkError:
      metric_result = PAYMENTS_RESULT_NETWORK_ERROR;
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      DCHECK_EQ(card_type, PaymentsRpcCardType::kVirtualCard);
      metric_result = PAYMENTS_RESULT_VCN_RETRIEVAL_TRY_AGAIN_FAILURE;
      break;
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, PaymentsRpcCardType::kVirtualCard);
      metric_result = PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE;
      break;
    case PaymentsRpcResult::kClientSideTimeout:
      metric_result = PAYMENTS_RESULT_CLIENT_SIDE_TIMEOUT;
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  std::string card_type_suffix;
  switch (card_type) {
    case PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case PaymentsRpcCardType::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  base::UmaHistogramEnumeration("Autofill.UnmaskPrompt.GetRealPanResult",
                                metric_result);

  base::UmaHistogramEnumeration(
      "Autofill.UnmaskPrompt.GetRealPanResult." + card_type_suffix,
      metric_result);
}

// static
void AutofillMetrics::LogRealPanDuration(base::TimeDelta duration,
                                         PaymentsRpcResult result,
                                         PaymentsRpcCardType card_type) {
  std::string result_suffix;
  std::string card_type_suffix;

  switch (card_type) {
    case PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case PaymentsRpcCardType::kUnknown:
      // Unknown card types imply UnmaskCardRequest::ParseResponse() was never
      // called, due to bad internet connection or otherwise. Log anyway so that
      // we have a rough idea of the magnitude of this problem.
      card_type_suffix = "UnknownCard";
      break;
  }

  switch (result) {
    case PaymentsRpcResult::kSuccess:
      result_suffix = "Success";
      break;
    case PaymentsRpcResult::kTryAgainFailure:
    case PaymentsRpcResult::kPermanentFailure:
      result_suffix = "Failure";
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, PaymentsRpcCardType::kVirtualCard);
      result_suffix = "VcnRetrievalFailure";
      break;
    case PaymentsRpcResult::kNetworkError:
      result_suffix = "NetworkError";
      break;
    case PaymentsRpcResult::kClientSideTimeout:
      result_suffix = "ClientSideTimeout";
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.GetRealPanDuration",
                              duration);
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.GetRealPanDuration." +
                                  card_type_suffix + "." + result_suffix,
                              duration);
}

// static
void AutofillMetrics::LogUnmaskingDuration(base::TimeDelta duration,
                                           PaymentsRpcResult result,
                                           PaymentsRpcCardType card_type) {
  std::string result_suffix;
  std::string card_type_suffix;

  switch (card_type) {
    case PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case PaymentsRpcCardType::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  switch (result) {
    case PaymentsRpcResult::kSuccess:
      result_suffix = "Success";
      break;
    case PaymentsRpcResult::kTryAgainFailure:
    case PaymentsRpcResult::kPermanentFailure:
      result_suffix = "Failure";
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, PaymentsRpcCardType::kVirtualCard);
      result_suffix = "VcnRetrievalFailure";
      break;
    case PaymentsRpcResult::kNetworkError:
      result_suffix = "NetworkError";
      break;
    case PaymentsRpcResult::kClientSideTimeout:
      result_suffix = "ClientSideTimeout";
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.UnmaskingDuration",
                              duration);
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.UnmaskingDuration." +
                                  card_type_suffix + "." + result_suffix,
                              duration);
}

// static
void AutofillMetrics::LogDeveloperEngagementMetric(
    DeveloperEngagementMetric metric) {
  DCHECK_LT(metric, NUM_DEVELOPER_ENGAGEMENT_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.DeveloperEngagement", metric,
                            NUM_DEVELOPER_ENGAGEMENT_METRICS);
}

// static
void AutofillMetrics::LogHeuristicPredictionQualityMetrics(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_HEURISTIC, field.heuristic_type(),
      form_interactions_ukm_logger, form, field, metric_type,
      /*log_rationalization_metrics=*/false);
}

// static
void AutofillMetrics::LogMlPredictionQualityMetrics(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_ML_PREDICTIONS,
      field.heuristic_type(HeuristicSource::kMachineLearning),
      form_interactions_ukm_logger, form, field, metric_type,
      /*log_rationalization_metrics=*/false);
}

// static
void AutofillMetrics::LogServerPredictionQualityMetrics(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(PREDICTION_SOURCE_SERVER, field.server_type(),
                              form_interactions_ukm_logger, form, field,
                              metric_type,
                              /*log_rationalization_metrics=*/false);
}

// static
void AutofillMetrics::LogOverallPredictionQualityMetrics(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_OVERALL, field.Type().GetStorableType(),
      form_interactions_ukm_logger, form, field, metric_type,
      /*log_rationalization_metrics=*/true);
}

void AutofillMetrics::LogEmailFieldPredictionMetrics(
    const AutofillField& field) {
  // If the field has no value, there is no need to record any of the metrics.
  const std::u16string& value = field.value_for_import();
  if (value.empty()) {
    return;
  }

  bool is_valid_email = IsValidEmailAddress(value);
  bool is_email_prediction = field.Type().GetStorableType() == EMAIL_ADDRESS;

  if (is_email_prediction) {
    EmailPredictionConfusionMatrix prediction_precision =
        is_valid_email ? EmailPredictionConfusionMatrix::kTruePositive
                       : EmailPredictionConfusionMatrix::kFalsePositive;
    base::UmaHistogramEnumeration(
        "Autofill.EmailPredictionCorrectness.Precision", prediction_precision);
  }

  if (is_valid_email) {
    EmailPredictionConfusionMatrix prediction_recall =
        is_email_prediction ? EmailPredictionConfusionMatrix::kTruePositive
                            : EmailPredictionConfusionMatrix::kFalseNegative;
    base::UmaHistogramEnumeration("Autofill.EmailPredictionCorrectness.Recall",
                                  prediction_recall);
  }
}

// static
void AutofillMetrics::LogEditedAutofilledFieldAtSubmission(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field) {
  const std::string aggregate_histogram =
      "Autofill.EditedAutofilledFieldAtSubmission2.Aggregate";
  const std::string type_specific_histogram =
      "Autofill.EditedAutofilledFieldAtSubmission2.ByFieldType";

  AutofilledFieldUserEditingStatusMetric editing_metric =
      field.previously_autofilled()
          ? AutofilledFieldUserEditingStatusMetric::AUTOFILLED_FIELD_WAS_EDITED
          : AutofilledFieldUserEditingStatusMetric::
                AUTOFILLED_FIELD_WAS_NOT_EDITED;

  // Record the aggregated UMA statistics.
  base::UmaHistogramEnumeration(aggregate_histogram, editing_metric);

  // Record the type specific UMA statistics.
  base::UmaHistogramSparse(type_specific_histogram,
                           GetFieldTypeUserEditStatusMetric(
                               field.Type().GetStorableType(), editing_metric));

  // Record the UMA statistics spliced by the autocomplete attribute value.
  FormType form_type = FieldTypeGroupToFormType(field.Type().group());
  if (form_type == FormType::kAddressForm ||
      form_type == FormType::kCreditCardForm) {
    bool autocomplete_off = field.autocomplete_attribute() == "off";
    const std::string autocomplete_histogram = base::StrCat(
        {"Autofill.Autocomplete.", autocomplete_off ? "Off" : "NotOff",
         ".EditedAutofilledFieldAtSubmission2.",
         form_type == FormType::kAddressForm ? "Address" : "CreditCard"});
    base::UmaHistogramEnumeration(autocomplete_histogram, editing_metric);
  }

  // If the field was edited, record the event to UKM.
  if (editing_metric ==
      AutofilledFieldUserEditingStatusMetric::AUTOFILLED_FIELD_WAS_EDITED) {
    form_interactions_ukm_logger->LogEditedAutofilledFieldAtSubmission(form,
                                                                       field);
  }
}

// static
void AutofillMetrics::LogServerQueryMetric(ServerQueryMetric metric) {
  DCHECK_LT(metric, NUM_SERVER_QUERY_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.ServerQueryResponse", metric,
                            NUM_SERVER_QUERY_METRICS);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(
    base::TimeDelta duration) {
  LogFormFillDuration("Autofill.FillDuration.FromLoad.WithAutofill", duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(
    base::TimeDelta duration) {
  LogFormFillDuration("Autofill.FillDuration.FromLoad.WithoutAutofill",
                      duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadForOneTimeCode(
    base::TimeDelta duration) {
  LogFormFillDuration("Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad",
                      duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromInteraction(
    const DenseSet<FormType>& form_types,
    bool used_autofill,
    base::TimeDelta duration) {
  std::string parent_metric;
  if (used_autofill) {
    parent_metric = "Autofill.FillDuration.FromInteraction.WithAutofill";
  } else {
    parent_metric = "Autofill.FillDuration.FromInteraction.WithoutAutofill";
  }
  LogFormFillDuration(parent_metric, duration);
  if (base::Contains(form_types, FormType::kCreditCardForm)) {
    LogFormFillDuration(parent_metric + ".CreditCard", duration);
  }
  if (base::Contains(form_types, FormType::kAddressForm)) {
    LogFormFillDuration(parent_metric + ".Address", duration);
  }
  if (base::Contains(form_types, FormType::kPasswordForm)) {
    LogFormFillDuration(parent_metric + ".Password", duration);
  }
  if (base::Contains(form_types, FormType::kUnknownFormType)) {
    LogFormFillDuration(parent_metric + ".Unknown", duration);
  }
}

// static
void AutofillMetrics::LogFormFillDurationFromInteractionForOneTimeCode(
    base::TimeDelta duration) {
  LogFormFillDuration(
      "Autofill.WebOTP.OneTimeCode.FillDuration.FromInteraction", duration);
}

// static
void AutofillMetrics::LogFormFillDuration(const std::string& metric,
                                          base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(metric, duration, base::Milliseconds(100),
                                base::Minutes(10), 50);
}

// static
void AutofillMetrics::LogStoredCreditCardMetrics(
    const std::vector<std::unique_ptr<CreditCard>>& local_cards,
    const std::vector<std::unique_ptr<CreditCard>>& server_cards,
    size_t server_card_count_with_card_art_image,
    base::TimeDelta disused_data_threshold) {
  size_t num_local_cards = 0;
  size_t num_local_cards_with_nickname = 0;
  size_t num_local_cards_with_invalid_number = 0;
  size_t num_server_cards = 0;
  size_t num_server_cards_with_nickname = 0;
  size_t num_disused_local_cards = 0;
  size_t num_disused_server_cards = 0;

  // Concatenate the local and server cards into one big collection of raw
  // CreditCard pointers.
  std::vector<const CreditCard*> credit_cards;
  credit_cards.reserve(local_cards.size() + server_cards.size());
  for (const auto* collection : {&local_cards, &server_cards}) {
    for (const auto& card : *collection) {
      credit_cards.push_back(card.get());
    }
  }

  // Iterate over all of the cards and gather metrics.
  const base::Time now = AutofillClock::Now();
  for (const CreditCard* card : credit_cards) {
    const base::TimeDelta time_since_last_use = now - card->use_date();
    const int days_since_last_use = time_since_last_use.InDays();
    const int disused_delta =
        (time_since_last_use > disused_data_threshold) ? 1 : 0;
    UMA_HISTOGRAM_COUNTS_1000("Autofill.DaysSinceLastUse.StoredCreditCard",
                              days_since_last_use);
    switch (card->record_type()) {
      case CreditCard::RecordType::kLocalCard:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Local",
            days_since_last_use);
        num_local_cards += 1;
        num_disused_local_cards += disused_delta;
        if (card->HasNonEmptyValidNickname())
          num_local_cards_with_nickname += 1;
        if (!card->HasValidCardNumber()) {
          num_local_cards_with_invalid_number += 1;
        }
        break;
      case CreditCard::RecordType::kMaskedServerCard:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server",
            days_since_last_use);
        num_server_cards += 1;
        num_disused_server_cards += disused_delta;
        if (card->HasNonEmptyValidNickname())
          num_server_cards_with_nickname += 1;
        break;
      case CreditCard::RecordType::kFullServerCard:
      case CreditCard::RecordType::kVirtualCard:
        // These card types are not persisted in Chrome.
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  // Calculate some summary info.
  const size_t num_cards = num_local_cards + num_server_cards;
  const size_t num_disused_cards =
      num_disused_local_cards + num_disused_server_cards;

  // Log the overall counts.
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount", num_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Local",
                            num_local_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Local.WithNickname",
                            num_local_cards_with_nickname);
  UMA_HISTOGRAM_COUNTS_1000(
      "Autofill.StoredCreditCardCount.Local.WithInvalidCardNumber",
      num_local_cards_with_invalid_number);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Server",
                            num_server_cards);
  UMA_HISTOGRAM_COUNTS_1000(
      "Autofill.StoredCreditCardCount.Server.WithNickname",
      num_server_cards_with_nickname);

  // For card types held by the user, log how many are disused.
  if (num_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount",
                              num_disused_cards);
  }
  if (num_local_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount.Local",
                              num_disused_local_cards);
  }
  if (num_server_cards) {
    UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardDisusedCount.Server",
                              num_disused_server_cards);
  }

  // Log the number of server cards that are enrolled with virtual cards.
  size_t virtual_card_enabled_card_count = base::ranges::count_if(
      server_cards, [](const std::unique_ptr<CreditCard>& card) {
        return card->virtual_card_enrollment_state() ==
               CreditCard::VirtualCardEnrollmentState::kEnrolled;
      });
  base::UmaHistogramCounts1000(
      "Autofill.StoredCreditCardCount.Server.WithVirtualCardMetadata",
      virtual_card_enabled_card_count);

  // Log the number of server cards that have valid customized art images.
  base::UmaHistogramCounts1000(
      "Autofill.StoredCreditCardCount.Server.WithCardArtImage",
      server_card_count_with_card_art_image);
}

// static
void AutofillMetrics::LogNumberOfCreditCardsSuppressedForDisuse(
    size_t num_cards) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.CreditCardsSuppressedForDisuse",
                            num_cards);
}

// static
void AutofillMetrics::LogNumberOfCreditCardsDeletedForDisuse(size_t num_cards) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.CreditCardsDeletedForDisuse", num_cards);
}

// static
void AutofillMetrics::LogNumberOfProfilesAtAutofillableFormSubmission(
    size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1M(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", num_profiles);
}

// static
void AutofillMetrics::LogNumberOfAddressesSuppressedForDisuse(
    size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.AddressesSuppressedForDisuse",
                            num_profiles);
}

// static
void AutofillMetrics::LogAutofillSuggestionHidingReason(
    FillingProduct filling_product,
    SuggestionHidingReason reason) {
  base::UmaHistogramEnumeration("Autofill.PopupHidingReason", reason);
  base::UmaHistogramEnumeration(base::StrCat({
                                    "Autofill.PopupHidingReason.",
                                    FillingProductToString(filling_product),
                                }),
                                reason);
}

void AutofillMetrics::LogPopupInteraction(FillingProduct filling_product,
                                          int popup_level,
                                          PopupInteraction action) {
  // Three level popups are the most we can currently have.
  CHECK_GE(popup_level, 0);
  CHECK_LE(popup_level, 2);
  const std::string histogram_metric_name =
      base::StrCat({"Autofill.PopupInteraction.PopupLevel.",
                    base::NumberToString(popup_level)});
  base::UmaHistogramEnumeration(histogram_metric_name, action);
  base::UmaHistogramEnumeration(
      base::StrCat({histogram_metric_name, ".",
                    FillingProductToString(filling_product)}),
      action);

  // Only emit user actions for address suggestions.
  if (filling_product != FillingProduct::kAddress) {
    return;
  }

  if (popup_level == 0) {
    switch (action) {
      case PopupInteraction::kPopupShown:
        base::RecordAction(base::UserMetricsAction(kUserActionRootPopupShown));
        break;
      case PopupInteraction::kSuggestionSelected:
        base::RecordAction(
            base::UserMetricsAction(kUserActionRootPopupSuggestionSelected));
        break;
      case PopupInteraction::kSuggestionAccepted:
        base::RecordAction(
            base::UserMetricsAction(kUserActionRootPopupSuggestionAccepted));
        break;
    }
  } else if (popup_level == 1) {
    switch (action) {
      case PopupInteraction::kPopupShown:
        base::RecordAction(
            base::UserMetricsAction(kUserActionSecondLevelPopupShown));
        break;
      case PopupInteraction::kSuggestionSelected:
        base::RecordAction(base::UserMetricsAction(
            kUserActionSecondLevelPopupSuggestionSelected));
        break;
      case PopupInteraction::kSuggestionAccepted:
        base::RecordAction(base::UserMetricsAction(
            kUserActionSecondLevelPopupSuggestionAccepted));
        break;
    }
  } else {
    switch (action) {
      case PopupInteraction::kPopupShown:
        base::RecordAction(
            base::UserMetricsAction(kUserActionThirdLevelPopupShown));
        break;
      case PopupInteraction::kSuggestionSelected:
        base::RecordAction(base::UserMetricsAction(
            kUserActionThirdLevelPopupSuggestionSelected));
        break;
      case PopupInteraction::kSuggestionAccepted:
        base::RecordAction(base::UserMetricsAction(
            kUserActionThirdLevelPopupSuggestionAccepted));
        break;
    }
  }
}

void AutofillMetrics::LogSectioningMetrics(
    const base::flat_map<Section, size_t>& fields_per_section) {
  constexpr std::string_view kBaseHistogramName = "Autofill.Sectioning.";
  UMA_HISTOGRAM_COUNTS_100(
      base::StrCat({kBaseHistogramName, "NumberOfSections"}),
      fields_per_section.size());
  for (auto& [_, section_size] : fields_per_section) {
    UMA_HISTOGRAM_COUNTS_100(
        base::StrCat({kBaseHistogramName, "FieldsPerSection"}), section_size);
  }
}

// static
void AutofillMetrics::LogServerResponseHasDataForForm(bool has_data) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.ServerResponseHasDataForForm", has_data);
}

// static
void AutofillMetrics::LogAutofillPerfectFilling(bool is_address,
                                                bool perfect_filling) {
  if (is_address) {
    UMA_HISTOGRAM_BOOLEAN("Autofill.PerfectFilling.Addresses", perfect_filling);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Autofill.PerfectFilling.CreditCards",
                          perfect_filling);
  }
}

AutofillMetrics::CreditCardSeamlessness::CreditCardSeamlessness(
    const FieldTypeSet& filled_types)
    : name_(filled_types.contains(CREDIT_CARD_NAME_FULL) ||
            (filled_types.contains(CREDIT_CARD_NAME_FIRST) &&
             filled_types.contains(CREDIT_CARD_NAME_LAST))),
      number_(filled_types.contains(CREDIT_CARD_NUMBER)),
      exp_(filled_types.contains(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR) ||
           filled_types.contains(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) ||
           (filled_types.contains(CREDIT_CARD_EXP_MONTH) &&
            (filled_types.contains(CREDIT_CARD_EXP_2_DIGIT_YEAR) ||
             filled_types.contains(CREDIT_CARD_EXP_4_DIGIT_YEAR)))),
      cvc_(filled_types.contains(CREDIT_CARD_VERIFICATION_CODE)) {}

AutofillMetrics::CreditCardSeamlessness::Metric
AutofillMetrics::CreditCardSeamlessness::QualitativeMetric() const {
  DCHECK(is_valid());
  if (name_ && number_ && exp_ && cvc_) {
    return Metric::kFullFill;
  } else if (!name_ && number_ && exp_ && cvc_) {
    return Metric::kOptionalNameMissing;
  } else if (name_ && number_ && exp_ && !cvc_) {
    return Metric::kOptionalCvcMissing;
  } else if (!name_ && number_ && exp_ && !cvc_) {
    return Metric::kOptionalNameAndCvcMissing;
  } else if (name_ && number_ && !exp_ && cvc_) {
    return Metric::kFullFillButExpDateMissing;
  } else {
    return Metric::kPartialFill;
  }
}

autofill_metrics::FormEvent
AutofillMetrics::CreditCardSeamlessness::QualitativeFillableFormEvent() const {
  DCHECK(is_valid());
  switch (QualitativeMetric()) {
    case Metric::kFullFill:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_FULL_FILL;
    case Metric::kOptionalNameMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_OPTIONAL_NAME_MISSING;
    case Metric::kFullFillButExpDateMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_FULL_FILL_BUT_EXPDATE_MISSING;
    case Metric::kOptionalNameAndCvcMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_OPTIONAL_NAME_AND_CVC_MISSING;
    case Metric::kOptionalCvcMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_OPTIONAL_CVC_MISSING;
    case Metric::kPartialFill:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_PARTIAL_FILL;
  }
  NOTREACHED_IN_MIGRATION();
  return autofill_metrics::
      FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_PARTIAL_FILL;
}

autofill_metrics::FormEvent
AutofillMetrics::CreditCardSeamlessness::QualitativeFillFormEvent() const {
  DCHECK(is_valid());
  switch (QualitativeMetric()) {
    case Metric::kFullFill:
      return autofill_metrics::FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_FULL_FILL;
    case Metric::kOptionalNameMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_OPTIONAL_NAME_MISSING;
    case Metric::kFullFillButExpDateMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_FULL_FILL_BUT_EXPDATE_MISSING;
    case Metric::kOptionalNameAndCvcMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_OPTIONAL_NAME_AND_CVC_MISSING;
    case Metric::kOptionalCvcMissing:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_OPTIONAL_CVC_MISSING;
    case Metric::kPartialFill:
      return autofill_metrics::
          FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_PARTIAL_FILL;
  }
  NOTREACHED_IN_MIGRATION();
  return autofill_metrics::FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_PARTIAL_FILL;
}

uint8_t AutofillMetrics::CreditCardSeamlessness::BitmaskMetric() const {
  DCHECK(is_valid());
  uint32_t bitmask = (name_ << 3) | (number_ << 2) | (exp_ << 1) | (cvc_ << 0);
  DCHECK_GE(bitmask, 1u);
  DCHECK_LE(bitmask, BitmaskExclusiveMax());
  return bitmask;
}

// static
void AutofillMetrics::LogCreditCardSeamlessnessAtFillTime(
    const LogCreditCardSeamlessnessParam& p) {
  auto GetSeamlessness = [&p](bool only_newly_filled_fields,
                              bool only_after_security_policy,
                              bool only_visible_fields) {
    FieldTypeSet autofilled_types;
    for (const auto& field : *p.form) {
      FieldGlobalId id = field->global_id();
      if (only_newly_filled_fields && !p.newly_filled_fields->contains(id))
        continue;
      if (only_after_security_policy && !p.safe_fields->contains(id))
        continue;
      if (only_visible_fields && !field->is_visible()) {
        continue;
      }
      autofilled_types.insert(field->Type().GetStorableType());
    }
    return CreditCardSeamlessness(autofilled_types);
  };

  auto RecordUma = [](std::string_view infix, CreditCardSeamlessness s) {
    std::string prefix = base::StrCat({"Autofill.CreditCard.Seamless", infix});
    base::UmaHistogramEnumeration(prefix, s.QualitativeMetric());
    base::UmaHistogramExactLinear(prefix + ".Bitmask", s.BitmaskMetric(),
                                  s.BitmaskExclusiveMax());
  };

  if (auto s = GetSeamlessness(false, false, false)) {
    RecordUma("Fillable.AtFillTimeBeforeSecurityPolicy", s);
    p.builder->SetFillable_BeforeSecurity_Bitmask(s.BitmaskMetric());
    p.builder->SetFillable_BeforeSecurity_Qualitative(
        s.QualitativeMetricAsInt());
    p.event_logger->Log(s.QualitativeFillableFormEvent(), *p.form);
  }
  if (auto s = GetSeamlessness(false, true, false)) {
    RecordUma("Fillable.AtFillTimeAfterSecurityPolicy", s);
    p.builder->SetFillable_AfterSecurity_Bitmask(s.BitmaskMetric());
    p.builder->SetFillable_AfterSecurity_Qualitative(
        s.QualitativeMetricAsInt());
  }
  if (auto s = GetSeamlessness(true, false, false)) {
    RecordUma("Fills.AtFillTimeBeforeSecurityPolicy", s);
    p.builder->SetFilled_BeforeSecurity_Bitmask(s.BitmaskMetric());
    p.builder->SetFilled_BeforeSecurity_Qualitative(s.QualitativeMetricAsInt());
  }
  if (auto s = GetSeamlessness(true, true, false)) {
    RecordUma("Fills.AtFillTimeAfterSecurityPolicy", s);
    p.builder->SetFilled_AfterSecurity_Bitmask(s.BitmaskMetric());
    p.builder->SetFilled_AfterSecurity_Qualitative(s.QualitativeMetricAsInt());
    p.event_logger->Log(s.QualitativeFillFormEvent(), *p.form);
  }
  if (auto s = GetSeamlessness(false, false, true)) {
    RecordUma("Fillable.AtFillTimeBeforeSecurityPolicy.Visible", s);
    p.builder->SetFillable_BeforeSecurity_Visible_Bitmask(s.BitmaskMetric());
    p.builder->SetFillable_BeforeSecurity_Visible_Qualitative(
        s.QualitativeMetricAsInt());
  }
  if (auto s = GetSeamlessness(false, true, true)) {
    RecordUma("Fillable.AtFillTimeAfterSecurityPolicy.Visible", s);
    p.builder->SetFillable_AfterSecurity_Visible_Bitmask(s.BitmaskMetric());
    p.builder->SetFillable_AfterSecurity_Visible_Qualitative(
        s.QualitativeMetricAsInt());
  }
  if (auto s = GetSeamlessness(true, false, true)) {
    RecordUma("Fills.AtFillTimeBeforeSecurityPolicy.Visible", s);
    p.builder->SetFilled_BeforeSecurity_Visible_Bitmask(s.BitmaskMetric());
    p.builder->SetFilled_BeforeSecurity_Visible_Qualitative(
        s.QualitativeMetricAsInt());
  }
  if (auto s = GetSeamlessness(true, true, true)) {
    RecordUma("Fills.AtFillTimeAfterSecurityPolicy.Visible", s);
    p.builder->SetFilled_AfterSecurity_Visible_Bitmask(s.BitmaskMetric());
    p.builder->SetFilled_AfterSecurity_Visible_Qualitative(
        s.QualitativeMetricAsInt());
  }

  // In a multi-frame form, a cross-origin field is filled only if
  // shared-autofill is enabled in the field's frame. Here, we log whether
  // shared-autofill did or would improve the fill seamlessness.
  //
  // This is referring to the actual fill, not the hypothetical scenarios
  // assuming that the card on file is complete or that there's no security
  // policy.
  //
  // See FormForest::GetRendererFormsOfBrowserForm() for details when a field
  // requires shared-autofill in order to be autofilled.
  //
  // Shared-autofill is a policy-controlled feature. As such, a parent frame
  // can enable it in a child frame with in the iframe's "allow" attribute:
  // <iframe allow="shared-autofill">. Whether it's enabled in the main frame is
  // controller by an HTTP header; by default, it is.
  auto RequiresSharedAutofill = [&](const AutofillField& field) {
    auto IsSensitiveFieldType = [](FieldType field_type) {
      switch (field_type) {
        case CREDIT_CARD_TYPE:
        case CREDIT_CARD_NAME_FULL:
        case CREDIT_CARD_NAME_FIRST:
        case CREDIT_CARD_NAME_LAST:
          return false;
        default:
          return true;
      }
    };
    const url::Origin& main_origin = p.form->main_frame_origin();
    const url::Origin& triggered_origin = p.field->origin();
    return field.origin() != triggered_origin &&
           (field.origin() != main_origin ||
            IsSensitiveFieldType(field.Type().GetStorableType())) &&
           triggered_origin == main_origin;
  };

  bool some_field_needs_shared_autofill = false;
  bool some_field_has_shared_autofill = false;
  for (const auto& field : *p.form) {
    if (RequiresSharedAutofill(*field) &&
        p.newly_filled_fields->contains(field->global_id())) {
      if (!p.safe_fields->contains(field->global_id())) {
        some_field_needs_shared_autofill = true;
      } else {
        some_field_has_shared_autofill = true;
      }
    }
  }

  enum SharedAutofillMetric : uint64_t {
    kSharedAutofillIsIrrelevant = 0,
    kSharedAutofillWouldHelp = 1,
    kSharedAutofillDidHelp = 2,
  };
  if (some_field_needs_shared_autofill) {
    p.builder->SetSharedAutofill(kSharedAutofillWouldHelp);
    p.event_logger->Log(
        autofill_metrics::FORM_EVENT_CREDIT_CARD_MISSING_SHARED_AUTOFILL,
        *p.form);
  } else if (some_field_has_shared_autofill) {
    p.builder->SetSharedAutofill(kSharedAutofillDidHelp);
  } else {
    p.builder->SetSharedAutofill(kSharedAutofillIsIrrelevant);
  }
}

// static
void AutofillMetrics::LogCreditCardSeamlessnessAtSubmissionTime(
    const FieldTypeSet& autofilled_types) {
  CreditCardSeamlessness seamlessness(autofilled_types);
  if (seamlessness.is_valid()) {
    base::UmaHistogramExactLinear(
        "Autofill.CreditCard.SeamlessFills.AtSubmissionTime.Bitmask",
        seamlessness.BitmaskMetric(), seamlessness.BitmaskExclusiveMax());
    base::UmaHistogramEnumeration(
        "Autofill.CreditCard.SeamlessFills.AtSubmissionTime",
        seamlessness.QualitativeMetric());
  }
}

// static
void AutofillMetrics::LogParseFormTiming(base::TimeDelta duration) {
  UMA_HISTOGRAM_TIMES("Autofill.Timing.ParseForm", duration);
}

// static
void AutofillMetrics::LogParsedFormUntilInteractionTiming(
    base::TimeDelta duration) {
  base::UmaHistogramLongTimes("Autofill.Timing.ParseFormUntilInteraction2",
                              duration);
}

// static
void AutofillMetrics::LogIsQueriedCreditCardFormSecure(bool is_secure) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.QueriedCreditCardFormIsSecure", is_secure);
}

// static
void AutofillMetrics::LogShowedHttpNotSecureExplanation() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_ShowedHttpNotSecureExplanation"));
}

// static
void AutofillMetrics::LogAutocompleteDaysSinceLastUse(size_t days) {
  UMA_HISTOGRAM_COUNTS_1000("Autocomplete.DaysSinceLastUse", days);
}

// static
void AutofillMetrics::OnAutocompleteSuggestionsShown() {
  AutofillMetrics::LogAutocompleteEvent(
      AutocompleteEvent::AUTOCOMPLETE_SUGGESTIONS_SHOWN);
}

// static
void AutofillMetrics::OnAutocompleteSuggestionDeleted(
    SingleEntryRemovalMethod removal_method) {
  AutofillMetrics::LogAutocompleteEvent(
      AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED);
  base::UmaHistogramEnumeration(
      "Autofill.Autocomplete.SingleEntryRemovalMethod", removal_method);
}

// static
void AutofillMetrics::LogAutocompleteEvent(AutocompleteEvent event) {
  DCHECK_LT(event, AutocompleteEvent::NUM_AUTOCOMPLETE_EVENTS);
  base::UmaHistogramEnumeration("Autocomplete.Events2", event,
                                NUM_AUTOCOMPLETE_EVENTS);
}

// static
void AutofillMetrics::LogAutofillPopupVisibleDuration(
    FillingProduct filling_product,
    base::TimeDelta duration) {
  base::UmaHistogramTimes("Autofill.Popup.VisibleDuration", duration);
  base::UmaHistogramTimes(
      base::StrCat({"Autofill.Popup.VisibleDuration.",
                    FillingProductToString(filling_product)}),
      duration);
}

// static
const char* AutofillMetrics::SubmissionSourceToUploadEventMetric(
    SubmissionSource source) {
  switch (source) {
    case SubmissionSource::NONE:
      return "Autofill.UploadEvent.None";
    case SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      return "Autofill.UploadEvent.SameDocumentNavigation";
    case SubmissionSource::XHR_SUCCEEDED:
      return "Autofill.UploadEvent.XhrSucceeded";
    case SubmissionSource::FRAME_DETACHED:
      return "Autofill.UploadEvent.FrameDetached";
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return "Autofill.UploadEvent.ProbablyFormSubmitted";
    case SubmissionSource::FORM_SUBMISSION:
      return "Autofill.UploadEvent.FormSubmission";
    case SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
      return "Autofill.UploadEvent.DomMutationAfterAutofill";
  }
  // Unit tests exercise this path, so do not put NOTREACHED() here.
  return "Autofill.UploadEvent.Unknown";
}

// static
void AutofillMetrics::LogUploadEvent(SubmissionSource submission_source,
                                     bool was_sent) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.UploadEvent", was_sent);
  base::UmaHistogramEnumeration(
      SubmissionSourceToUploadEventMetric(submission_source),
      was_sent ? UploadEventStatus::kSent : UploadEventStatus::kNotSent);
}

// static
void AutofillMetrics::LogDeveloperEngagementUkm(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    const GURL& url,
    bool is_for_credit_card,
    DenseSet<FormTypeNameForLogging> form_types,
    int developer_engagement_metrics,
    FormSignature form_signature) {
  DCHECK(developer_engagement_metrics);
  DCHECK_LT(developer_engagement_metrics,
            1 << NUM_DEVELOPER_ENGAGEMENT_METRICS);
  if (!url.is_valid())
    return;

  ukm::builders::Autofill_DeveloperEngagement(source_id)
      .SetDeveloperEngagement(developer_engagement_metrics)
      .SetIsForCreditCard(is_for_credit_card)
      .SetFormTypes(FormTypesToBitVector(form_types))
      .SetFormSignature(HashFormSignature(form_signature))
      .Record(ukm_recorder);
}

AutofillMetrics::FormInteractionsUkmLogger::FormInteractionsUkmLogger(
    AutofillClient* autofill_client,
    ukm::UkmRecorder* ukm_recorder)
    : autofill_client_(autofill_client), ukm_recorder_(ukm_recorder) {}

ukm::builders::Autofill_CreditCardFill
AutofillMetrics::FormInteractionsUkmLogger::CreateCreditCardFillBuilder() {
  return ukm::builders::Autofill_CreditCardFill(GetSourceId());
}

void AutofillMetrics::FormInteractionsUkmLogger::Record(
    ukm::builders::Autofill_CreditCardFill&& builder) {
  if (CanLog())
    builder.Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::OnFormsParsed(
    const ukm::SourceId source_id) {
  if (!CanLog())
    return;

  source_id_ = source_id;
}

void AutofillMetrics::FormInteractionsUkmLogger::LogInteractedWithForm(
    bool is_for_credit_card,
    size_t local_record_type_count,
    size_t server_record_type_count,
    FormSignature form_signature) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_InteractedWithForm(GetSourceId())
      .SetIsForCreditCard(is_for_credit_card)
      .SetLocalRecordTypeCount(local_record_type_count)
      .SetServerRecordTypeCount(server_record_type_count)
      .SetFormSignature(HashFormSignature(form_signature))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogSuggestionsShown(
    const FormStructure& form,
    const AutofillField& field,
    base::TimeTicks form_parsed_timestamp,
    bool off_the_record) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_SuggestionsShown(GetSourceId())
      .SetHeuristicType(static_cast<int>(field.heuristic_type()))
      .SetHtmlFieldType(static_cast<int>(field.html_type()))
      .SetServerType(static_cast<int>(field.server_type()))
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .Record(ukm_recorder_);

  base::UmaHistogramBoolean("Autofill.SuggestionShown.OffTheRecord",
                            off_the_record);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogDidFillSuggestion(
    const FormStructure& form,
    const AutofillField& field,
    std::optional<CreditCard::RecordType> record_type) {
  if (!CanLog())
    return;

  auto metric = ukm::builders::Autofill_SuggestionFilled(GetSourceId());
  if (record_type) {
    metric.SetRecordType(base::to_underlying(*record_type));
  }
  metric.SetIsForCreditCard(record_type.has_value())
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form.form_parsed_timestamp()))
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::
    LogEditedAutofilledFieldAtSubmission(const FormStructure& form,
                                         const AutofillField& field) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_EditedAutofilledFieldAtSubmission(GetSourceId())
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetOverallType(static_cast<int64_t>(field.Type().GetStorableType()))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogTextFieldDidChange(
    const FormStructure& form,
    const AutofillField& field) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_TextFieldDidChange(GetSourceId())
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFieldTypeGroup(static_cast<int>(field.Type().group()))
      .SetHeuristicType(static_cast<int>(field.heuristic_type()))
      .SetServerType(static_cast<int>(field.server_type()))
      .SetHtmlFieldType(static_cast<int>(field.html_type()))
      .SetHtmlFieldMode(static_cast<int>(field.html_mode()))
      .SetIsAutofilled(field.is_autofilled())
      .SetIsEmpty(field.value(ValueSemantics::kCurrent).empty())
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form.form_parsed_timestamp()))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogFieldFillStatus(
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_FieldFillStatus(GetSourceId())
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form.form_parsed_timestamp()))
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetValidationEvent(static_cast<int64_t>(metric_type))
      .SetIsAutofilled(static_cast<int64_t>(field.is_autofilled()))
      .SetWasPreviouslyAutofilled(
          static_cast<int64_t>(field.previously_autofilled()))
      .Record(ukm_recorder_);
}

// TODO(szhangcs): Take FormStructure and AutofillField and extract
// FormSignature and TimeTicks inside the function.
void AutofillMetrics::FormInteractionsUkmLogger::LogFieldType(
    base::TimeTicks form_parsed_timestamp,
    FormSignature form_signature,
    FieldSignature field_signature,
    QualityMetricPredictionSource prediction_source,
    QualityMetricType metric_type,
    FieldType predicted_type,
    FieldType actual_type) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_FieldTypeValidation(GetSourceId())
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .SetFormSignature(HashFormSignature(form_signature))
      .SetFieldSignature(HashFieldSignature(field_signature))
      .SetValidationEvent(static_cast<int64_t>(metric_type))
      .SetPredictionSource(static_cast<int64_t>(prediction_source))
      .SetPredictedType(static_cast<int64_t>(predicted_type))
      .SetActualType(static_cast<int64_t>(actual_type))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::
    LogAutofillFieldInfoAtFormRemove(
        const FormStructure& form,
        const AutofillField& field,
        AutofillMetrics::AutocompleteState autocomplete_state) {
  if (!CanLog()) {
    return;
  }

  const std::vector<AutofillField::FieldLogEventType>& field_log_events =
      field.field_log_events();
  if (field_log_events.empty()) {
    return;
  }

  // Set the fields with autofill information according to Autofill2.FieldInfo
  // UKM schema:
  // https://docs.google.com/document/d/1ZH0JbL6bES3cD4KqZWsGR6n8I-rhnkx6no6nQOgYq5w/
  OptionalBoolean was_focused_by_tap_or_click = OptionalBoolean::kFalse;
  OptionalBoolean suggestion_was_available = OptionalBoolean::kUndefined;
  OptionalBoolean suggestion_was_shown = OptionalBoolean::kUndefined;
  OptionalBoolean suggestion_was_accepted = OptionalBoolean::kUndefined;

  // Records whether this field was autofilled before checking the iframe
  // security policy.
  OptionalBoolean was_autofilled_before_security_policy =
      OptionalBoolean::kUndefined;
  OptionalBoolean had_value_before_filling = OptionalBoolean::kUndefined;
  DenseSet<FieldFillingSkipReason> autofill_skipped_status;
  size_t autofill_count = 0;

  OptionalBoolean user_typed_into_field = OptionalBoolean::kFalse;
  OptionalBoolean filled_value_was_modified = OptionalBoolean::kUndefined;
  OptionalBoolean had_typed_or_filled_value_at_submission =
      OptionalBoolean::kUndefined;
  OptionalBoolean had_value_after_filling = OptionalBoolean::kUndefined;
  OptionalBoolean has_value_after_typing = OptionalBoolean::kUndefined;

  // Records whether filling was ever prevented because of the cross iframe
  // autofill security policy that applies to credit cards.
  OptionalBoolean filling_prevented_by_iframe_security_policy =
      OptionalBoolean::kUndefined;
  // Records whether this field was actually safely autofilled after checking
  // the iframe security policy.
  OptionalBoolean was_autofilled_after_security_policy =
      OptionalBoolean::kUndefined;

  // TODO(crbug.com/40225658): Add a metric in |FieldInfo| UKM event to indicate
  // whether the user had any data available for the respective field type.

  // If multiple fields have the same signature, this indicates the position
  // within this set of fields. This allows us to understand problems related
  // to duplicated field signatures.
  size_t rank_in_field_signature_group = 0;

  // Field types from local heuristics prediction.
  // The field type from the active local heuristic pattern.
  FieldType heuristic_type = UNKNOWN_TYPE;
  // The type of the field predicted from patterns whose stability is above
  // suspicion.
  FieldType heuristic_legacy_type = UNKNOWN_TYPE;
  // The type of the field predicted from the source of local heuristics on
  // the client, which uses patterns applied for most users.
  FieldType heuristic_default_type = UNKNOWN_TYPE;
  // The type of the field predicted from the heuristics that uses experimental
  // patterns.
  FieldType heuristic_experimental_type = UNKNOWN_TYPE;

  // Field types from Autocomplete attribute.
  // Information of the HTML autocomplete attribute, see
  // components/autofill/core/common/mojom/autofill_types.mojom.
  HtmlFieldMode html_mode = HtmlFieldMode::kNone;
  HtmlFieldType html_type = HtmlFieldType::kUnrecognized;

  // The field type predicted by the Autofill crowdsourced server from
  // majority voting.
  std::optional<FieldType> server_type1 = std::nullopt;
  FieldPrediction::Source prediction_source1 =
      FieldPrediction::SOURCE_UNSPECIFIED;
  std::optional<FieldType> server_type2 = std::nullopt;
  FieldPrediction::Source prediction_source2 =
      FieldPrediction::SOURCE_UNSPECIFIED;
  // This is an annotation for server predicted field types which indicates
  // that a manual override defines the server type.
  bool server_type_is_override = false;

  // The final field type from the list of |autofill::FieldType| that we
  // choose after rationalization, which is used to determine
  // the autofill suggestion when the user triggers autofilling.
  FieldType overall_type = NO_SERVER_DATA;
  // The sections are mapped to consecutive natural numbers starting at 1,
  // numbered according to the ordering of their first fields.
  size_t section_id = 0;
  bool type_changed_by_rationalization = false;

  bool had_heuristic_type = false;
  bool had_html_type = false;
  bool had_server_type = false;
  bool had_rationalization_event = false;

  DenseSet<AutofillMetrics::AutofillStatus> autofill_status_vector;
  auto SetStatusVector = [&autofill_status_vector](
                             AutofillMetrics::AutofillStatus status,
                             bool value) {
    DCHECK(!autofill_status_vector.contains(status));
    if (value) {
      autofill_status_vector.insert(status);
    }
  };

  for (const auto& log_event : field_log_events) {
    static_assert(absl::variant_size<AutofillField::FieldLogEventType>() == 10,
                  "When adding new variants check that this function does not "
                  "need to be updated.");
    if (auto* event =
            absl::get_if<AskForValuesToFillFieldLogEvent>(&log_event)) {
      was_focused_by_tap_or_click = OptionalBoolean::kTrue;
      suggestion_was_available |= event->has_suggestion;
      suggestion_was_shown |= event->suggestion_is_shown;
      if (suggestion_was_shown == OptionalBoolean::kTrue &&
          suggestion_was_accepted == OptionalBoolean::kUndefined) {
        // Initialize suggestion_was_accepted to a defined value when the first
        // time the suggestion is shown.
        suggestion_was_accepted = OptionalBoolean::kFalse;
      }
    }

    if (auto* event = absl::get_if<TriggerFillFieldLogEvent>(&log_event)) {
      // Ignore events which are not address or credit card fill events.
      if (event->data_type != FillDataType::kAutofillProfile &&
          event->data_type != FillDataType::kCreditCard) {
        continue;
      }
      suggestion_was_accepted = OptionalBoolean::kTrue;
    }

    if (auto* event = absl::get_if<FillFieldLogEvent>(&log_event)) {
      was_autofilled_before_security_policy |=
          event->was_autofilled_before_security_policy;
      had_value_before_filling |= event->had_value_before_filling;
      autofill_skipped_status.insert(event->autofill_skipped_status);
      had_value_after_filling = event->had_value_after_filling;
      if (was_autofilled_before_security_policy == OptionalBoolean::kTrue &&
          filled_value_was_modified == OptionalBoolean::kUndefined) {
        // Initialize filled_value_was_modified to a defined value when the
        // field is filled for the first time.
        filled_value_was_modified = OptionalBoolean::kFalse;
      }

      filling_prevented_by_iframe_security_policy |=
          OptionalBoolean(event->filling_prevented_by_iframe_security_policy ==
                          OptionalBoolean::kTrue);
      was_autofilled_after_security_policy |=
          OptionalBoolean(event->filling_prevented_by_iframe_security_policy ==
                          OptionalBoolean::kFalse);
      ++autofill_count;
    }

    if (auto* event = absl::get_if<TypingFieldLogEvent>(&log_event)) {
      user_typed_into_field = OptionalBoolean::kTrue;
      if (was_autofilled_after_security_policy == OptionalBoolean::kTrue) {
        filled_value_was_modified = OptionalBoolean::kTrue;
      }
      has_value_after_typing = event->has_value_after_typing;
    }

    if (auto* event =
            absl::get_if<HeuristicPredictionFieldLogEvent>(&log_event)) {
      switch (event->heuristic_source) {
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
        case HeuristicSource::kLegacyRegexes:
          heuristic_legacy_type = event->field_type;
          break;
#else
        case HeuristicSource::kDefaultRegexes:
          heuristic_default_type = event->field_type;
          break;
        case HeuristicSource::kExperimentalRegexes:
          heuristic_experimental_type = event->field_type;
          break;
        case HeuristicSource::kPredictionImprovementRegexes:
          // Prediction improvements are currently ignored for Autofill based
          // UKM logging.
          break;
#endif
        case HeuristicSource::kMachineLearning:
          NOTREACHED();
      }

      if (event->is_active_heuristic_source) {
        heuristic_type = event->field_type;
      }
      rank_in_field_signature_group = event->rank_in_field_signature_group;
      had_heuristic_type = true;
    }

    if (auto* event =
            absl::get_if<AutocompleteAttributeFieldLogEvent>(&log_event)) {
      html_type = event->html_type;
      html_mode = event->html_mode;
      rank_in_field_signature_group = event->rank_in_field_signature_group;
      had_html_type = true;
    }

    if (auto* event = absl::get_if<ServerPredictionFieldLogEvent>(&log_event)) {
      server_type1 = event->server_type1;
      prediction_source1 = event->prediction_source1;
      server_type2 = event->server_type2;
      prediction_source2 = event->prediction_source2;
      server_type_is_override = event->server_type_prediction_is_override;
      rank_in_field_signature_group = event->rank_in_field_signature_group;
      had_server_type = true;
    }

    if (auto* event = absl::get_if<RationalizationFieldLogEvent>(&log_event)) {
      overall_type = event->field_type;
      section_id = event->section_id;
      type_changed_by_rationalization = event->type_changed;
      had_rationalization_event = true;
    }
  }

  if (had_value_after_filling != OptionalBoolean::kUndefined ||
      has_value_after_typing != OptionalBoolean::kUndefined) {
    had_typed_or_filled_value_at_submission =
        ToOptionalBoolean(had_value_after_filling == OptionalBoolean::kTrue ||
                          has_value_after_typing == OptionalBoolean::kTrue);
  }

  ukm::builders::Autofill2_FieldInfo builder(GetSourceId());
  builder
      .SetFormSessionIdentifier(
          AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id()))
      .SetFieldSessionIdentifier(
          AutofillMetrics::FieldGlobalIdToHash64Bit(field.global_id()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFormControlType2(base::to_underlying(field.form_control_type()))
      .SetAutocompleteState(base::to_underlying(autocomplete_state))
      .SetFieldLogEventCount(field_log_events.size());

  SetStatusVector(AutofillStatus::kIsFocusable, field.IsFocusable());
  SetStatusVector(AutofillStatus::kUserTypedIntoField,
                  OptionalBooleanToBool(user_typed_into_field));
  SetStatusVector(AutofillStatus::kWasFocused, field.was_focused());
  SetStatusVector(AutofillStatus::kWasFocusedByTapOrClick,
                  OptionalBooleanToBool(was_focused_by_tap_or_click));
  SetStatusVector(AutofillStatus::kIsInSubFrame,
                  form.global_id().frame_token != field.host_frame());

  if (filling_prevented_by_iframe_security_policy !=
      OptionalBoolean::kUndefined) {
    SetStatusVector(
        AutofillStatus::kFillingPreventedByIframeSecurityPolicy,
        OptionalBooleanToBool(filling_prevented_by_iframe_security_policy));
  }

  if (was_focused_by_tap_or_click == OptionalBoolean::kTrue) {
    SetStatusVector(AutofillStatus::kSuggestionWasAvailable,
                    OptionalBooleanToBool(suggestion_was_available));
    SetStatusVector(AutofillStatus::kSuggestionWasShown,
                    OptionalBooleanToBool(suggestion_was_shown));
  }

  if (suggestion_was_shown == OptionalBoolean::kTrue) {
    SetStatusVector(AutofillStatus::kSuggestionWasAccepted,
                    OptionalBooleanToBool(suggestion_was_accepted));
  }

  SetStatusVector(AutofillStatus::kWasAutofillTriggered, autofill_count > 0);
  if (autofill_count > 0) {
    SetStatusVector(
        AutofillStatus::kWasAutofilledBeforeSecurityPolicy,
        OptionalBooleanToBool(was_autofilled_before_security_policy));
    SetStatusVector(AutofillStatus::kHadValueBeforeFilling,
                    OptionalBooleanToBool(had_value_before_filling));
    SetStatusVector(AutofillStatus::kWasRefill, autofill_count > 1);
    if (was_autofilled_after_security_policy != OptionalBoolean::kUndefined) {
      SetStatusVector(
          AutofillStatus::kWasAutofilledAfterSecurityPolicy,
          OptionalBooleanToBool(was_autofilled_after_security_policy));
    }

    static_assert(autofill_skipped_status.data().size() == 1);
    builder.SetAutofillSkippedStatus(autofill_skipped_status.data()[0]);
  }

  if (filled_value_was_modified != OptionalBoolean::kUndefined) {
    SetStatusVector(AutofillStatus::kFilledValueWasModified,
                    OptionalBooleanToBool(filled_value_was_modified));
  }

  if (had_typed_or_filled_value_at_submission != OptionalBoolean::kUndefined) {
    SetStatusVector(
        AutofillStatus::kHadTypedOrFilledValueAtSubmission,
        OptionalBooleanToBool(had_typed_or_filled_value_at_submission));
  }

  if (had_heuristic_type) {
    builder.SetHeuristicType(heuristic_type)
        .SetHeuristicTypeLegacy(heuristic_legacy_type)
        .SetHeuristicTypeDefault(heuristic_default_type)
        .SetHeuristicTypeExperimental(heuristic_experimental_type);
  }

  if (had_html_type) {
    builder.SetHtmlFieldType(base::to_underlying(html_type))
        .SetHtmlFieldMode(base::to_underlying(html_mode));
  }

  if (had_server_type) {
    int64_t server_type1_value = server_type1.has_value()
                                     ? server_type1.value()
                                     : /*SERVER_RESPONSE_PENDING*/ 161;
    int64_t server_type2_value = server_type2.has_value()
                                     ? server_type2.value()
                                     : /*SERVER_RESPONSE_PENDING*/ 161;
    builder.SetServerType1(server_type1_value)
        .SetServerPredictionSource1(prediction_source1)
        .SetServerType2(server_type2_value)
        .SetServerPredictionSource2(prediction_source2)
        .SetServerTypeIsOverride(server_type_is_override);
  }

  if (had_rationalization_event) {
    builder.SetOverallType(overall_type)
        .SetSectionId(section_id)
        .SetTypeChangedByRationalization(type_changed_by_rationalization);
  }

  if (rank_in_field_signature_group) {
    builder.SetRankInFieldSignatureGroup(rank_in_field_signature_group);
  }

  // Serialize the DenseSet of the autofill status into int64_t.
  static_assert(autofill_status_vector.data().size() == 1U);
  builder.SetAutofillStatusVector(autofill_status_vector.data()[0]);

  builder.Record(ukm_recorder_);
}

// static
void AutofillMetrics::LogAutofillFieldInfoAfterSubmission(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    const FormStructure& form,
    base::TimeTicks form_submitted_timestamp) {
  for (const auto& field : form) {
    // The possible field submitted types determined by comparing the submitted
    // value in the field with the data stored in the Autofill server. We will
    // have at most three possible field submitted types.
    FieldType submitted_type1 = UNKNOWN_TYPE;

    ukm::builders::Autofill2_FieldInfoAfterSubmission builder(source_id);
    builder
        .SetFormSessionIdentifier(
            AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id()))
        .SetFieldSessionIdentifier(
            AutofillMetrics::FieldGlobalIdToHash64Bit(field->global_id()));

    const FieldTypeSet& type_set = field->possible_types();
    if (!type_set.empty()) {
      auto type = type_set.begin();
      submitted_type1 = *type;
      if (type_set.size() >= 2) {
        builder.SetSubmittedType2(*++type);
      }
      if (type_set.size() >= 3) {
        builder.SetSubmittedType3(*++type);
      }
    }

    builder.SetSubmittedType1(submitted_type1)
        .SetSubmissionSource(static_cast<int>(form.submission_source()))
        .SetMillisecondsFromFormParsedUntilSubmission(
            GetSemanticBucketMinForAutofillDurationTiming(
                (form_submitted_timestamp - form.form_parsed_timestamp())
                    .InMilliseconds()))
        .Record(ukm_recorder);
  }
}

void AutofillMetrics::FormInteractionsUkmLogger::
    LogAutofillFormSummaryAtFormRemove(
        const FormStructure& form_structure,
        FormEventSet form_events,
        base::TimeTicks initial_interaction_timestamp,
        base::TimeTicks form_submitted_timestamp) {
  if (!CanLog()) {
    return;
  }

  static_assert(form_events.data().size() == 2U,
                "If you add a new form event, you need to create a new "
                "AutofillFormEvents metric in Autofill2.FormSummary");
  ukm::builders::Autofill2_FormSummary builder(GetSourceId());
  builder
      .SetFormSessionIdentifier(
          AutofillMetrics::FormGlobalIdToHash64Bit(form_structure.global_id()))
      .SetFormSignature(HashFormSignature(form_structure.form_signature()))
      .SetAutofillFormEvents(form_events.data()[0])
      .SetAutofillFormEvents2(form_events.data()[1])
      .SetWasSubmitted(!form_submitted_timestamp.is_null())
      .SetSampleRate(1);

  if (!form_submitted_timestamp.is_null() &&
      !form_structure.form_parsed_timestamp().is_null() &&
      form_submitted_timestamp > form_structure.form_parsed_timestamp()) {
    builder.SetMillisecondsFromFormParsedUntilSubmission(
        GetSemanticBucketMinForAutofillDurationTiming(
            (form_submitted_timestamp - form_structure.form_parsed_timestamp())
                .InMilliseconds()));
  }

  if (!form_submitted_timestamp.is_null() &&
      !initial_interaction_timestamp.is_null() &&
      form_submitted_timestamp > initial_interaction_timestamp) {
    builder.SetMillisecondsFromFirstInteratctionUntilSubmission(
        GetSemanticBucketMinForAutofillDurationTiming(
            (form_submitted_timestamp - initial_interaction_timestamp)
                .InMilliseconds()));
  }
  builder.Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::
    LogAutofillFormWithExperimentalFieldsCountAtFormRemove(
        const FormStructure& form_structure) {
  if (!CanLog()) {
    return;
  }

  // Number of non-empty experimental fields found for each of the 5 buckets.
  std::array<int, 5> num_experimental_fields = {0, 0, 0, 0, 0};

  // Build icu::RegexPattern* from experiment parameters.
  static base::NoDestructor<AutofillRegexCache> regex_cache(ThreadSafe(false));
  auto compile_pattern =
      [](const std::string& pattern) -> const icu::RegexPattern* {
    return pattern.empty()
               ? nullptr
               : regex_cache->GetRegexPattern(base::UTF8ToUTF16(pattern));
  };
  std::array<const icu::RegexPattern*, 5> kRegexPatterns = {
      compile_pattern(features::kAutofillUKMExperimentalFieldsBucket0.Get()),
      compile_pattern(features::kAutofillUKMExperimentalFieldsBucket1.Get()),
      compile_pattern(features::kAutofillUKMExperimentalFieldsBucket2.Get()),
      compile_pattern(features::kAutofillUKMExperimentalFieldsBucket3.Get()),
      compile_pattern(features::kAutofillUKMExperimentalFieldsBucket4.Get())};

  // Determine whether `pattern` matches `value`.
  auto matches = [](const std::u16string& value,
                    const icu::RegexPattern& pattern) {
    return !value.empty() && autofill::MatchesRegex(value, pattern);
  };
  // Count in `num_experimental_fields[i]` if `pattern[i]` matches the label,
  // id_attribute or name_attribute of `field`. Returns true if any pattern
  // matched.
  auto count_experimental_field = [&](const AutofillField& field) {
    bool found_experimental_fields = false;
    for (size_t i = 0; i < kRegexPatterns.size(); ++i) {
      const icu::RegexPattern* pattern = kRegexPatterns[i];
      if (pattern && (matches(field.label(), *pattern) ||
                      matches(field.id_attribute(), *pattern) ||
                      matches(field.name_attribute(), *pattern))) {
        ++num_experimental_fields[i];
        found_experimental_fields = true;
      }
    }
    return found_experimental_fields;
  };

  // Count which patterns matched for fields that were non-empty and had a
  // typing or filling event.
  bool found_experimental_fields = false;
  for (const std::unique_ptr<AutofillField>& field : form_structure.fields()) {
    OptionalBoolean has_typed_or_filled_value_at_submission =
        OptionalBoolean::kUndefined;

    const std::vector<AutofillField::FieldLogEventType>& field_log_events =
        field->field_log_events();

    for (const AutofillField::FieldLogEventType& log_event : field_log_events) {
      if (auto* event = absl::get_if<FillFieldLogEvent>(&log_event)) {
        if (event->filling_prevented_by_iframe_security_policy ==
            OptionalBoolean::kFalse) {
          has_typed_or_filled_value_at_submission =
              event->had_value_after_filling;
        }
      }

      if (auto* event = absl::get_if<TypingFieldLogEvent>(&log_event)) {
        has_typed_or_filled_value_at_submission = event->has_value_after_typing;
      }
    }

    // The value of has_typed_or_filled_value_at_submission does not capture
    // correctly if javascript clears a field. It only indicates that the last
    // user action (filling or autofill) led to a value.
    if (has_typed_or_filled_value_at_submission == OptionalBoolean::kTrue) {
      found_experimental_fields |= count_experimental_field(*field);
    }
  }

  // Report the results.
  if (found_experimental_fields) {
    ukm::builders::Autofill2_SubmittedFormWithExperimentalFields builder(
        GetSourceId());
    builder
        .SetFormSessionIdentifier(AutofillMetrics::FormGlobalIdToHash64Bit(
            form_structure.global_id()))
        .SetFormSignature(HashFormSignature(form_structure.form_signature()));
    if (num_experimental_fields[0]) {
      builder.SetNumberOfNonEmptyExperimentalFields0(
          num_experimental_fields[0]);
    }
    if (num_experimental_fields[1]) {
      builder.SetNumberOfNonEmptyExperimentalFields1(
          num_experimental_fields[1]);
    }
    if (num_experimental_fields[2]) {
      builder.SetNumberOfNonEmptyExperimentalFields2(
          num_experimental_fields[2]);
    }
    if (num_experimental_fields[3]) {
      builder.SetNumberOfNonEmptyExperimentalFields3(
          num_experimental_fields[3]);
    }
    if (num_experimental_fields[4]) {
      builder.SetNumberOfNonEmptyExperimentalFields4(
          num_experimental_fields[4]);
    }
    builder.Record(ukm_recorder_);
  }
}

void AutofillMetrics::FormInteractionsUkmLogger::
    LogFocusedComplexFormAtFormRemove(
        const FormStructure& form_structure,
        FormEventSet form_events,
        base::TimeTicks initial_interaction_timestamp,
        base::TimeTicks form_submitted_timestamp) {
  if (!CanLog()) {
    return;
  }

  DenseSet<FormTypeNameForLogging> form_type_names_for_logging =
      autofill_metrics::GetFormTypesForLogging(form_structure);

  // To save bandwidth, only forms are reported that are a
  // kPostalAddressForm or a kCreditCardForm.
  if (!form_type_names_for_logging.contains_any(
          {FormTypeNameForLogging::kPostalAddressForm,
           FormTypeNameForLogging::kCreditCardForm})) {
    return;
  }

  // Whether a field whose type group was not FormType::kUnknownFormType
  // was focused.
  bool some_classified_field_was_focused = false;

  // The set of form types of fields that were focused via a tap or click and
  // therefore eligible for autofill.
  DenseSet<FormType> autofill_data_queried;
  // The set of form types of fields for which suggestions were shown.
  DenseSet<FormType> suggestions_available;
  // The set of form types of fields that the user modified (filled, pasted,
  // edited).
  DenseSet<FormType> user_modified;
  // The set of form types of fields that were autofilled (at some point).
  DenseSet<FormType> autofilled;
  // The set of form types of fields that were edited after they were
  // autofilled.
  DenseSet<FormType> edited_after_autofill;
  // The set of form types of fields that were non-empty at submission time.
  DenseSet<FormType> had_non_empty_value_at_submission;

  DenseSet<FormType> control_group_of_ablation;
  DenseSet<FormType> ablation_group_of_ablation;
  DenseSet<FormType> control_group_of_conditional_ablation;
  DenseSet<FormType> ablation_group_of_conditional_ablation;
  int day_in_ablation_window = -1;

  for (const std::unique_ptr<AutofillField>& field : form_structure.fields()) {
    FormType form_type = FieldTypeGroupToFormType(field->Type().group());
    if (form_type == FormType::kUnknownFormType) {
      continue;
    }

    some_classified_field_was_focused |= field->was_focused();

    OptionalBoolean had_value_after_filling = OptionalBoolean::kUndefined;
    OptionalBoolean has_value_after_typing = OptionalBoolean::kUndefined;

    const std::vector<AutofillField::FieldLogEventType>& field_log_events =
        field->field_log_events();

    bool current_field_was_autofilled = false;
    for (const AutofillField::FieldLogEventType& log_event : field_log_events) {
      if (auto* event =
              absl::get_if<AskForValuesToFillFieldLogEvent>(&log_event)) {
        autofill_data_queried.insert(form_type);
        if (event->has_suggestion == OptionalBoolean::kTrue) {
          suggestions_available.insert(form_type);
        }
      }

      if (auto* event = absl::get_if<FillFieldLogEvent>(&log_event)) {
        if (event->filling_prevented_by_iframe_security_policy ==
            OptionalBoolean::kFalse) {
          user_modified.insert(form_type);
          autofilled.insert(form_type);
          current_field_was_autofilled = true;
          had_value_after_filling = event->had_value_after_filling;
        }
      }

      if (auto* event = absl::get_if<TypingFieldLogEvent>(&log_event)) {
        user_modified.insert(form_type);
        if (current_field_was_autofilled) {
          edited_after_autofill.insert(form_type);
        }
        has_value_after_typing = event->has_value_after_typing;
      }

      if (auto* event = absl::get_if<AblationFieldLogEvent>(&log_event)) {
        if (event->ablation_group == AblationGroup::kControl) {
          control_group_of_ablation.insert(form_type);
        } else if (event->ablation_group == AblationGroup::kAblation) {
          ablation_group_of_ablation.insert(form_type);
        }
        if (event->conditional_ablation_group == AblationGroup::kControl) {
          control_group_of_conditional_ablation.insert(form_type);
        } else if (event->conditional_ablation_group ==
                   AblationGroup::kAblation) {
          ablation_group_of_conditional_ablation.insert(form_type);
        }
        if (event->day_in_ablation_window >= 0) {
          day_in_ablation_window = event->day_in_ablation_window;
        }
      }
    }

    if (had_value_after_filling == OptionalBoolean::kTrue ||
        has_value_after_typing == OptionalBoolean::kTrue) {
      had_non_empty_value_at_submission.insert(form_type);
    }
  }

  // TODO(crbug.com/348362142): DataAvailability

  // Don't log anything if the user did not interact with address or credit
  // card fields (or other fields that are not FormType::kUnknownFormType).
  if (!some_classified_field_was_focused) {
    return;
  }

  ukm::builders::Autofill2_FocusedComplexForm builder(GetSourceId());
  builder
      .SetFormSessionIdentifier(
          AutofillMetrics::FormGlobalIdToHash64Bit(form_structure.global_id()))
      .SetFormSignature(HashFormSignature(form_structure.form_signature()))
      .SetWasSubmitted(!form_submitted_timestamp.is_null())
      .SetAutofillDataQueried(autofill_data_queried.data()[0])
      .SetUserModified(user_modified.data()[0])
      .SetAutofilled(autofilled.data()[0])
      .SetEditedAfterAutofill(edited_after_autofill.data()[0])
      .SetSuggestionsAvailable(suggestions_available.data()[0])
      .SetHadNonEmptyValueAtSubmission(
          had_non_empty_value_at_submission.data()[0])
      .SetFormTypes(form_type_names_for_logging.data()[0]);

  if (!form_submitted_timestamp.is_null() &&
      !initial_interaction_timestamp.is_null() &&
      form_submitted_timestamp > initial_interaction_timestamp) {
    builder.SetMillisecondsFromFirstInteractionUntilSubmission(
        GetSemanticBucketMinForAutofillDurationTiming(
            (form_submitted_timestamp - initial_interaction_timestamp)
                .InMilliseconds()));
  }

  if (day_in_ablation_window >= 0) {
    builder.SetIsAblationStudyInDryRunMode(
        features::kAutofillAblationStudyIsDryRun.Get());
    builder.SetDayInAblationWindow(day_in_ablation_window);
    builder.SetIsInControlGroupOfAblation(control_group_of_ablation.data()[0]);
    builder.SetIsInAblationGroupOfAblation(
        ablation_group_of_ablation.data()[0]);
    builder.SetIsInControlGroupOfConditionalAblation(
        control_group_of_conditional_ablation.data()[0]);
    builder.SetIsInAblationGroupOfConditionalAblation(
        ablation_group_of_conditional_ablation.data()[0]);
  }

  builder.Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::
    LogHiddenRepresentationalFieldSkipDecision(const FormStructure& form,
                                               const AutofillField& field,
                                               bool is_skipped) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_HiddenRepresentationalFieldSkipDecision(GetSourceId())
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFieldTypeGroup(static_cast<int>(field.Type().group()))
      .SetFieldOverallType(static_cast<int>(field.Type().GetStorableType()))
      .SetHeuristicType(static_cast<int>(field.heuristic_type()))
      .SetServerType(static_cast<int>(field.server_type()))
      .SetHtmlFieldType(static_cast<int>(field.html_type()))
      .SetHtmlFieldMode(static_cast<int>(field.html_mode()))
      .SetIsSkipped(is_skipped)
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::
    LogRepeatedServerTypePredictionRationalized(
        const FormSignature form_signature,
        const AutofillField& field,
        FieldType old_type) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_RepeatedServerTypePredictionRationalized(
      GetSourceId())
      .SetFormSignature(HashFormSignature(form_signature))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFieldTypeGroup(static_cast<int>(field.Type().group()))
      .SetFieldNewOverallType(static_cast<int>(field.Type().GetStorableType()))
      .SetHeuristicType(static_cast<int>(field.heuristic_type()))
      .SetHtmlFieldType(static_cast<int>(field.html_type()))
      .SetHtmlFieldMode(static_cast<int>(field.html_mode()))
      .SetServerType(static_cast<int>(field.server_type()))
      .SetFieldOldOverallType(static_cast<int>(old_type))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogSectioningHash(
    FormSignature form_signature,
    uint32_t sectioning_signature) {
  ukm::builders::Autofill_Sectioning(GetSourceId())
      .SetFormSignature(HashFormSignature(form_signature))
      .SetSectioningSignature(sectioning_signature % 1024)
      .Record(ukm_recorder_);
}

ukm::SourceId AutofillMetrics::FormInteractionsUkmLogger::GetSourceId() {
  if (!source_id_.has_value()) {
    source_id_ = autofill_client_->GetUkmSourceId();
  }
  return *source_id_;
}

int64_t AutofillMetrics::FormTypesToBitVector(
    const DenseSet<FormTypeNameForLogging>& form_types) {
  int64_t form_type_bv = 0;
  for (FormTypeNameForLogging form_type : form_types) {
    DCHECK_LT(static_cast<int64_t>(form_type), 63);
    form_type_bv |= 1LL << static_cast<int64_t>(form_type);
  }
  return form_type_bv;
}

void AutofillMetrics::LogServerCardLinkClicked(PaymentsSigninState sync_state) {
  base::UmaHistogramEnumeration("Autofill.ServerCardLinkClicked", sync_state);
}

// static
const char* AutofillMetrics::GetMetricsSyncStateSuffix(
    PaymentsSigninState sync_state) {
  switch (sync_state) {
    case PaymentsSigninState::kSignedOut:
      return ".SignedOut";
    case PaymentsSigninState::kSignedIn:
      return ".SignedIn";
    case PaymentsSigninState::kSignedInAndWalletSyncTransportEnabled:
      return ".SignedInAndWalletSyncTransportEnabled";
    case PaymentsSigninState::kSignedInAndSyncFeatureEnabled:
      return ".SignedInAndSyncFeatureEnabled";
    case PaymentsSigninState::kSyncPaused:
      return ".SyncPaused";
    case PaymentsSigninState::kUnknown:
      return ".Unknown";
  }
}

void AutofillMetrics::FormInteractionsUkmLogger::LogKeyMetrics(
    const DenseSet<FormTypeNameForLogging>& form_types,
    bool data_to_fill_available,
    bool suggestions_shown,
    bool edited_autofilled_field,
    bool suggestion_filled,
    const FormInteractionCounts& form_interaction_counts,
    const FormInteractionsFlowId& flow_id,
    std::optional<int64_t> fast_checkout_run_id) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_KeyMetrics builder(GetSourceId());
  builder.SetFillingReadiness(data_to_fill_available)
      .SetFillingAssistance(suggestion_filled)
      .SetFormTypes(FormTypesToBitVector(form_types))
      .SetAutofillFills(form_interaction_counts.autofill_fills)
      .SetFormElementUserModifications(
          form_interaction_counts.form_element_user_modifications)
      .SetFlowId(flow_id.value());
  if (fast_checkout_run_id) {
    builder.SetFastCheckoutRunId(fast_checkout_run_id.value());
  }
  if (suggestions_shown)
    builder.SetFillingAcceptance(suggestion_filled);

  if (suggestion_filled)
    builder.SetFillingCorrectness(!edited_autofilled_field);

  builder.Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogFormEvent(
    autofill_metrics::FormEvent form_event,
    const DenseSet<FormTypeNameForLogging>& form_types,
    base::TimeTicks form_parsed_timestamp) {
  if (!CanLog())
    return;

  if (form_parsed_timestamp.is_null())
    return;

  ukm::builders::Autofill_FormEvent builder(GetSourceId());
  builder.SetAutofillFormEvent(static_cast<int>(form_event))
      .SetFormTypes(FormTypesToBitVector(form_types))
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .Record(ukm_recorder_);
}

bool AutofillMetrics::FormInteractionsUkmLogger::CanLog() const {
  return ukm_recorder_ != nullptr;
}

int64_t AutofillMetrics::FormInteractionsUkmLogger::MillisecondsSinceFormParsed(
    base::TimeTicks form_parsed_timestamp) const {
  DCHECK(!form_parsed_timestamp.is_null());
  // Use the pinned timestamp as the current time if it's set.
  base::TimeTicks now =
      pinned_timestamp_.is_null() ? base::TimeTicks::Now() : pinned_timestamp_;

  return ukm::GetExponentialBucketMin(
      (now - form_parsed_timestamp).InMilliseconds(),
      kAutofillEventDataBucketSpacing);
}

AutofillMetrics::UkmTimestampPin::UkmTimestampPin(
    FormInteractionsUkmLogger* logger)
    : logger_(logger) {
  DCHECK(logger_);
  DCHECK(!logger_->has_pinned_timestamp());
  logger_->set_pinned_timestamp(base::TimeTicks::Now());
}

AutofillMetrics::UkmTimestampPin::~UkmTimestampPin() {
  DCHECK(logger_->has_pinned_timestamp());
  logger_->set_pinned_timestamp(base::TimeTicks());
}

// static
void AutofillMetrics::LogWebOTPPhoneCollectionMetricStateUkm(
    ukm::UkmRecorder* recorder,
    ukm::SourceId source_id,
    uint32_t phone_collection_metric_state) {
  // UKM recording is not supported for WebViews.
  if (!recorder || source_id == ukm::kInvalidSourceId)
    return;

  ukm::builders::WebOTPImpact builder(source_id);
  builder.SetPhoneCollection(phone_collection_metric_state);
  builder.Record(recorder);
}

void AutofillMetrics::LogVerificationStatusOfNameTokensOnProfileUsage(
    const AutofillProfile& profile) {
  constexpr std::string_view base_histogram_name =
      "Autofill.NameTokenVerificationStatusAtProfileUsage.";

  for (const auto& [type, name] : kStructuredNameTypeToNameMap) {
    // Do not record the status for empty values.
    if (profile.GetRawInfo(type).empty()) {
      continue;
    }

    VerificationStatus status = profile.GetVerificationStatus(type);
    base::UmaHistogramEnumeration(base::StrCat({base_histogram_name, name}),
                                  status);
    base::UmaHistogramEnumeration(base::StrCat({base_histogram_name, "Any"}),
                                  status);
  }
}

void AutofillMetrics::LogVerificationStatusOfAddressTokensOnProfileUsage(
    const AutofillProfile& profile) {
  constexpr std::string_view base_histogram_name =
      "Autofill.AddressTokenVerificationStatusAtProfileUsage.";

  for (const auto& [type, name] : kStructuredAddressTypeToNameMap) {
    // Do not record the status for empty values.
    if (profile.GetRawInfo(type).empty()) {
      continue;
    }

    VerificationStatus status = profile.GetVerificationStatus(type);
    base::UmaHistogramEnumeration(base::StrCat({base_histogram_name, name}),
                                  status);
    base::UmaHistogramEnumeration(base::StrCat({base_histogram_name, "Any"}),
                                  status);
  }
}

// static
void AutofillMetrics::LogVirtualCardMetadataSynced(bool existing_card) {
  base::UmaHistogramBoolean("Autofill.VirtualCard.MetadataSynced",
                            existing_card);
}

// static
void AutofillMetrics::LogImageFetchResult(bool succeeded) {
  base::UmaHistogramBoolean("Autofill.ImageFetcher.Result", succeeded);
}

// static
void AutofillMetrics::LogImageFetcherRequestLatency(base::TimeDelta duration) {
  base::UmaHistogramLongTimes("Autofill.ImageFetcher.RequestLatency", duration);
}

// static
void AutofillMetrics::LogAutocompletePredictionCollisionState(
    PredictionState prediction_state,
    AutocompleteState autocomplete_state) {
  if (prediction_state == PredictionState::kNone &&
      autocomplete_state == AutocompleteState::kNone) {
    return;
  }
  // The buckets are calculated by using the least significant two bits to
  // encode the `autocomplete_state`, and the next two bits to encode the
  // `prediction_state`.
  int bucket = (static_cast<int>(prediction_state) << 2) |
               static_cast<int>(autocomplete_state);
  // Without (kNone, kNone), 4*4 - 1 = 15 possible pairs remain. Log the bucket
  // 0-based, in order to interpret the metric as an enum.
  DCHECK(1 <= bucket && bucket <= 15);
  UMA_HISTOGRAM_ENUMERATION("Autofill.Autocomplete.PredictionCollisionState",
                            bucket - 1, 15);
}

// static
void AutofillMetrics::LogAutocompletePredictionCollisionTypes(
    AutocompleteState autocomplete_state,
    FieldType server_type,
    FieldType heuristic_type) {
  // Convert `autocomplete_state` to a string for the metric's name.
  std::string autocomplete_suffix;
  switch (autocomplete_state) {
    case AutocompleteState::kNone:
      autocomplete_suffix = "None";
      break;
    case AutocompleteState::kValid:
      autocomplete_suffix = "Valid";
      break;
    case AutocompleteState::kGarbage:
      autocomplete_suffix = "Garbage";
      break;
    case AutocompleteState::kOff:
      autocomplete_suffix = "Off";
      break;
    case AutocompleteState::kPassword:
      autocomplete_suffix = "Password";
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  // Log the metric for heuristic and server type.
  std::string kHistogramName =
      "Autofill.Autocomplete.PredictionCollisionType2.";
  if (server_type != NO_SERVER_DATA) {
    base::UmaHistogramEnumeration(
        kHistogramName + "Server." + autocomplete_suffix, server_type,
        FieldType::MAX_VALID_FIELD_TYPE);
  }
  base::UmaHistogramEnumeration(
      kHistogramName + "Heuristics." + autocomplete_suffix, heuristic_type,
      FieldType::MAX_VALID_FIELD_TYPE);
  base::UmaHistogramEnumeration(
      kHistogramName + "ServerOrHeuristics." + autocomplete_suffix,
      server_type != NO_SERVER_DATA ? server_type : heuristic_type,
      FieldType::MAX_VALID_FIELD_TYPE);
}

const std::string PaymentsRpcResultToMetricsSuffix(PaymentsRpcResult result) {
  std::string result_suffix;

  switch (result) {
    case PaymentsRpcResult::kSuccess:
      result_suffix = ".Success";
      break;
    case PaymentsRpcResult::kTryAgainFailure:
    case PaymentsRpcResult::kPermanentFailure:
      result_suffix = ".Failure";
      break;
    case PaymentsRpcResult::kNetworkError:
      result_suffix = ".NetworkError";
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      result_suffix = ".VcnRetrievalFailure";
      break;
    case PaymentsRpcResult::kClientSideTimeout:
      result_suffix = ".ClientSideTimeout";
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED_IN_MIGRATION();
  }

  return result_suffix;
}

// static
std::string AutofillMetrics::GetHistogramStringForCardType(
    absl::variant<PaymentsRpcCardType, CreditCard::RecordType> card_type) {
  if (absl::holds_alternative<PaymentsRpcCardType>(card_type)) {
    switch (absl::get<PaymentsRpcCardType>(card_type)) {
      case PaymentsRpcCardType::kServerCard:
        return ".ServerCard";
      case PaymentsRpcCardType::kVirtualCard:
        return ".VirtualCard";
      case PaymentsRpcCardType::kUnknown:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  } else if (absl::holds_alternative<CreditCard::RecordType>(card_type)) {
    switch (absl::get<CreditCard::RecordType>(card_type)) {
      case CreditCard::RecordType::kFullServerCard:
      case CreditCard::RecordType::kMaskedServerCard:
        return ".ServerCard";
      case CreditCard::RecordType::kVirtualCard:
        return ".VirtualCard";
      case CreditCard::RecordType::kLocalCard:
        return ".LocalCard";
    }
  }

  return "";
}

// static
void AutofillMetrics::LogDeleteAddressProfileFromPopup() {
  // Only the "confirmed" bucket can be recorded, as the user cannot cancel this
  // type of deletion.
  base::UmaHistogramBoolean("Autofill.ProfileDeleted.Popup",
                            /*delete_confirmed=*/true);
  base::UmaHistogramBoolean("Autofill.ProfileDeleted.Any",
                            /*delete_confirmed=*/true);
}

// static
void AutofillMetrics::LogDeleteAddressProfileFromKeyboardAccessory() {
  // Only the "confirmed" bucket is recorded here, as the cancellation can only
  // be recorded from Java.
  base::UmaHistogramBoolean("Autofill.ProfileDeleted.KeyboardAccessory",
                            /*delete_confirmed=*/true);
  base::UmaHistogramBoolean("Autofill.ProfileDeleted.Any",
                            /*delete_confirmed=*/true);
}

// static
uint64_t AutofillMetrics::FormGlobalIdToHash64Bit(
    const FormGlobalId& form_global_id) {
  return StrToHash64Bit(
      base::NumberToString(form_global_id.renderer_id.value()) +
      form_global_id.frame_token.ToString());
}

// static
uint64_t AutofillMetrics::FieldGlobalIdToHash64Bit(
    const FieldGlobalId& field_global_id) {
  return StrToHash64Bit(
      base::NumberToString(field_global_id.renderer_id.value()) +
      field_global_id.frame_token.ToString());
}

}  // namespace autofill

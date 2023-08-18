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
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/language/core/browser/language_usage_metrics.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill {

using mojom::SubmissionSource;

namespace {

// Exponential bucket spacing for UKM event data.
constexpr double kAutofillEventDataBucketSpacing = 2.0;

// Overflow bucket for form user interactions
constexpr int64_t kFormUserInteractionsOverflowBucket = 20;

using autofill_metrics::FormGroupFillingStats;

// Translates structured name types into simple names that are used for
// naming histograms.
constexpr auto kStructuredNameTypeToNameMap =
    base::MakeFixedFlatMap<ServerFieldType, base::StringPiece>(
        {{NAME_FULL, "Full"},
         {NAME_FIRST, "First"},
         {NAME_MIDDLE, "Middle"},
         {NAME_LAST, "Last"},
         {NAME_LAST_FIRST, "FirstLast"},
         {NAME_LAST_SECOND, "SecondLast"}});

// Translates structured address types into simple names that are used for
// naming histograms.
constexpr auto kStructuredAddressTypeToNameMap =
    base::MakeFixedFlatMap<ServerFieldType, base::StringPiece>(
        {{ADDRESS_HOME_STREET_ADDRESS, "StreetAddress"},
         {ADDRESS_HOME_STREET_NAME, "StreetName"},
         {ADDRESS_HOME_HOUSE_NUMBER, "HouseNumber"},
         {ADDRESS_HOME_FLOOR, "FloorNumber"},
         {ADDRESS_HOME_APT_NUM, "ApartmentNumber"},
         {ADDRESS_HOME_PREMISE_NAME, "Premise"},
         {ADDRESS_HOME_SUBPREMISE, "SubPremise"}});

// Note: if adding an enum value here, update the corresponding description for
// AutofillFieldPredictionQualityByFieldType in
// tools/metrics/histograms/enums.xml.
enum FieldTypeGroupForMetrics {
  GROUP_AMBIGUOUS = 0,
  GROUP_NAME,
  GROUP_COMPANY,
  GROUP_ADDRESS_LINE_1,
  GROUP_ADDRESS_LINE_2,
  GROUP_ADDRESS_CITY,
  GROUP_ADDRESS_STATE,
  GROUP_ADDRESS_ZIP,
  GROUP_ADDRESS_COUNTRY,
  GROUP_ADDRESS_HOME_STREET_NAME,
  GROUP_ADDRESS_HOME_DEPENDENT_STREET_NAME,
  GROUP_ADDRESS_HOME_HOUSE_NUMBER,
  GROUP_ADDRESS_HOME_PREMISE_NAME,
  GROUP_ADDRESS_HOME_SUBPREMISE,
  GROUP_PHONE,
  GROUP_FAX,  // Deprecated.
  GROUP_EMAIL,
  GROUP_CREDIT_CARD_NAME,
  GROUP_CREDIT_CARD_NUMBER,
  GROUP_CREDIT_CARD_DATE,
  GROUP_CREDIT_CARD_TYPE,
  GROUP_PASSWORD,
  GROUP_ADDRESS_LINE_3,
  GROUP_USERNAME,
  GROUP_STREET_ADDRESS,
  GROUP_CREDIT_CARD_VERIFICATION,
  GROUP_UNFILLABLE,
  GROUP_ADDRESS_HOME_APT_NUM,
  GROUP_ADDRESS_HOME_SORTING_CODE,
  GROUP_ADDRESS_HOME_DEPENDENT_LOCALITY,
  GROUP_ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME,
  GROUP_ADDRESS_HOME_OTHER_SUBUNIT,
  GROUP_ADDRESS_HOME_ADDRESS,
  GROUP_ADDRESS_HOME_ADDRESS_WITH_NAME,
  GROUP_ADDRESS_HOME_FLOOR,
  GROUP_UNKNOWN_TYPE,
  GROUP_BIRTHDATE,
  GROUP_IBAN,
  GROUP_ADDRESS_HOME_LANDMARK,
  GROUP_ADDRESS_HOME_BETWEEN_STREETS,
  GROUP_ADDRESS_HOME_ADMIN_LEVEL2,
  GROUP_ADDRESS_HOME_STREET_LOCATION,
  GROUP_ADDRESS_HOME_OVERFLOW,
  GROUP_DELIVERY_INSTRUCTIONS,
  GROUP_ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
  GROUP_ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
  // Add new entries here and update enums.xml.
  NUM_FIELD_TYPE_GROUPS_FOR_METRICS
};

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
    ServerFieldType field_type,
    AutofillMetrics::FieldTypeQualityMetric metric) {
  DCHECK_LT(metric, AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS);

  FieldTypeGroupForMetrics group = GROUP_AMBIGUOUS;
  switch (AutofillType(field_type).group()) {
    case FieldTypeGroup::kNoGroup:
      group = GROUP_AMBIGUOUS;
      break;

    case FieldTypeGroup::kName:
    case FieldTypeGroup::kNameBilling:
      group = GROUP_NAME;
      break;

    case FieldTypeGroup::kCompany:
      group = GROUP_COMPANY;
      break;

    case FieldTypeGroup::kIban:
      group = GROUP_IBAN;
      break;

    case FieldTypeGroup::kAddressHome:
    case FieldTypeGroup::kAddressBilling:
      switch (AutofillType(field_type).GetStorableType()) {
        case ADDRESS_HOME_LINE1:
          group = GROUP_ADDRESS_LINE_1;
          break;
        case ADDRESS_HOME_LINE2:
          group = GROUP_ADDRESS_LINE_2;
          break;
        case ADDRESS_HOME_LINE3:
          group = GROUP_ADDRESS_LINE_3;
          break;
        case ADDRESS_HOME_APT_NUM:
          group = GROUP_ADDRESS_HOME_APT_NUM;
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
        case ADDRESS_HOME_DEPENDENT_STREET_NAME:
          group = GROUP_ADDRESS_HOME_DEPENDENT_STREET_NAME;
          break;
        case ADDRESS_HOME_HOUSE_NUMBER:
          group = GROUP_ADDRESS_HOME_HOUSE_NUMBER;
          break;
        case ADDRESS_HOME_PREMISE_NAME:
          group = GROUP_ADDRESS_HOME_PREMISE_NAME;
          break;
        case ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME:
          group = GROUP_ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME;
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
        case UPI_VPA:
        case NAME_LAST_FIRST:
        case NAME_LAST_CONJUNCTION:
        case NAME_LAST_SECOND:
        case NAME_HONORIFIC_PREFIX:
        case NAME_FULL_WITH_HONORIFIC_PREFIX:
        case BIRTHDATE_DAY:
        case BIRTHDATE_MONTH:
        case BIRTHDATE_4_DIGIT_YEAR:
        case IBAN_VALUE:
        case MAX_VALID_FIELD_TYPE:
        case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
        case SINGLE_USERNAME_FORGOT_PASSWORD:
          NOTREACHED() << field_type << " type is not in that group.";
          group = GROUP_AMBIGUOUS;
          break;
      }
      break;

    case FieldTypeGroup::kEmail:
      group = GROUP_EMAIL;
      break;

    case FieldTypeGroup::kPhoneHome:
    case FieldTypeGroup::kPhoneBilling:
      group = GROUP_PHONE;
      break;

    case FieldTypeGroup::kBirthdateField:
      group = GROUP_BIRTHDATE;
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
        case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
          group = GROUP_CREDIT_CARD_VERIFICATION;
          break;
        default:
          NOTREACHED() << field_type << " has no group assigned (ambiguous)";
          group = GROUP_AMBIGUOUS;
          break;
      }
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
      NOTREACHED();
      break;
  }

  // Use bits 8-15 for the group and bits 0-7 for the metric.
  static_assert(AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS <= UINT8_MAX,
                "maximum field type quality metric must fit into 8 bits");
  static_assert(NUM_FIELD_TYPE_GROUPS_FOR_METRICS <= UINT8_MAX,
                "number of field type groups must fit into 8 bits");
  return (group << 8) | metric;
}

// This function encodes the integer value of a |ServerFieldType| and the
// metric value of an |AutofilledFieldUserEditingStatus| into a 16 bit integer.
// The lower four bits are used to encode the editing status and the higher
// 12 bits are used to encode the field type.
int GetFieldTypeUserEditStatusMetric(
    ServerFieldType server_type,
    AutofillMetrics::AutofilledFieldUserEditingStatusMetric metric) {
  static_assert(ServerFieldType::MAX_VALID_FIELD_TYPE <= (UINT16_MAX >> 4),
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
    default:
    case AutofillMetrics::PREDICTION_SOURCE_UNKNOWN:
      NOTREACHED();
      return "Unknown";

    case AutofillMetrics::PREDICTION_SOURCE_HEURISTIC:
      return "Heuristic";
    case AutofillMetrics::PREDICTION_SOURCE_SERVER:
      return "Server";
    case AutofillMetrics::PREDICTION_SOURCE_OVERALL:
      return "Overall";
  }
}

const char* GetQualityMetricTypeSuffix(
    AutofillMetrics::QualityMetricType metric_type) {
  switch (metric_type) {
    default:
      NOTREACHED();
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
ServerFieldType GetActualFieldType(const ServerFieldTypeSet& possible_types,
                                   ServerFieldType predicted_type) {
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
  ServerFieldTypeSet collapsed_field_types;
  for (auto type : possible_types) {
    DCHECK_NE(type, EMPTY_TYPE);
    DCHECK_NE(type, UNKNOWN_TYPE);

    // A phone number that's only missing its country code is (for metrics
    // purposes) the same as the whole phone number.
    if (type == PHONE_HOME_CITY_AND_NUMBER)
      collapsed_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
    else
      collapsed_field_types.insert(AutofillType(type).GetStorableType());
  }

  // Capture the field's type, if it is unambiguous.
  ServerFieldType actual_type = AMBIGUOUS_TYPE;
  if (collapsed_field_types.size() == 1)
    actual_type = *collapsed_field_types.begin();

  DVLOG(2) << "Inferred Type: "
           << AutofillType::ServerFieldTypeToString(actual_type);
  return actual_type;
}

// Check if the value of |field| is same as one of the previously autofilled
// values. This indicates a bad rationalization if |field| has
// only_fill_when_focused set to true.
bool DuplicatedFilling(const FormStructure& form, const AutofillField& field) {
  for (const auto& form_field : form) {
    if (field.value == form_field->value && form_field->is_autofilled)
      return true;
  }
  return false;
}

void LogPredictionQualityMetricsForFieldsOnlyFilledWhenFocused(
    const std::string& aggregate_histogram,
    const std::string& type_specific_histogram,
    const std::string& rationalization_quality_histogram,
    ServerFieldType predicted_type,
    ServerFieldType actual_type,
    bool is_empty,
    bool is_ambiguous,
    bool log_rationalization_metrics,
    const FormStructure& form,
    const AutofillField& field) {
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
          rationalization_quality_histogram,
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
          rationalization_quality_histogram,
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
    base::UmaHistogramSparse(rationalization_quality_histogram,
                             AutofillMetrics::RATIONALIZATION_OK);
  }
  return;
}

void LogPredictionQualityMetricsForCommonFields(
    const std::string& aggregate_histogram,
    const std::string& type_specific_histogram,
    ServerFieldType predicted_type,
    ServerFieldType actual_type,
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
    ServerFieldType predicted_type,
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
  std::string rationalization_quality_histogram = base::JoinString(
      {"Autofill.RationalizationQuality.PhoneNumber", suffix}, "");

  const ServerFieldTypeSet& possible_types =
      metric_type == AutofillMetrics::TYPE_AUTOCOMPLETE_BASED
          ? ServerFieldTypeSet{AutofillType(field.html_type(),
                                            field.html_mode())
                                   .GetStorableType()}
          : field.possible_types();

  // Get the best type classification we can for the field.
  ServerFieldType actual_type =
      GetActualFieldType(possible_types, predicted_type);

  DVLOG(2) << "Predicted: "
           << AutofillType::ServerFieldTypeToString(predicted_type) << "; "
           << "Actual: " << AutofillType::ServerFieldTypeToString(actual_type);

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
    LogPredictionQualityMetricsForFieldsOnlyFilledWhenFocused(
        aggregate_histogram, type_specific_histogram,
        rationalization_quality_histogram, predicted_type, actual_type,
        is_empty, is_ambiguous, log_rationalization_metrics, form, field);
    return;
  }

  LogPredictionQualityMetricsForCommonFields(
      aggregate_histogram, type_specific_histogram, predicted_type, actual_type,
      is_empty, is_ambiguous);
}

}  // namespace

const int kMaxBucketsCount = 50;

// static
void AutofillMetrics::LogProfileSuggestionsMadeWithFormatter(
    bool made_with_formatter) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.ProfileSuggestionsMadeWithFormatter",
                        made_with_formatter);
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
    AutofillClient::SaveCreditCardOptions options) {
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

  if (options.from_dynamic_change_form) {
    base::UmaHistogramEnumeration(
        "Autofill.CreditCardInfoBar" + destination + ".FromDynamicChangeForm",
        metric, NUM_INFO_BAR_METRICS);
  }

  if (options.has_non_focusable_field) {
    base::UmaHistogramEnumeration(
        "Autofill.CreditCardInfoBar" + destination + ".FromNonFocusableForm",
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
}

// static
void AutofillMetrics::LogCreditCardFillingInfoBarMetric(InfoBarMetric metric) {
  DCHECK_LT(metric, NUM_INFO_BAR_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.CreditCardFillingInfoBar", metric,
                            NUM_INFO_BAR_METRICS);
}

// static
void AutofillMetrics::LogScanCreditCardPromptMetric(
    ScanCreditCardPromptMetric metric) {
  DCHECK_LT(metric, NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.ScanCreditCardPrompt", metric,
                            NUM_SCAN_CREDIT_CARD_PROMPT_METRICS);
}

// static
void AutofillMetrics::LogScanCreditCardCompleted(
    const base::TimeDelta& duration,
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
  std::string dialog_type;
  switch (autofill_progress_dialog_type) {
    case AutofillProgressDialogType::kAndroidFIDOProgressDialog:
      dialog_type = "AndroidFIDO";
      break;
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
      dialog_type = "CardUnmask";
      break;
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
      return;
  }
  base::UmaHistogramBoolean(
      "Autofill.ProgressDialog." + dialog_type + ".Result",
      is_canceled_by_user);
}

void AutofillMetrics::LogProgressDialogShown(
    AutofillProgressDialogType autofill_progress_dialog_type) {
  std::string dialog_type;
  switch (autofill_progress_dialog_type) {
    case AutofillProgressDialogType::kAndroidFIDOProgressDialog:
      dialog_type = "AndroidFIDO";
      break;
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
      dialog_type = "CardUnmask";
      break;
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
      return;
  }
  base::UmaHistogramBoolean("Autofill.ProgressDialog." + dialog_type + ".Shown",
                            true);
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
    const base::TimeDelta& duration,
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
      NOTREACHED();
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
void AutofillMetrics::LogTimeBeforeAbandonUnmasking(
    const base::TimeDelta& duration,
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
void AutofillMetrics::LogRealPanResult(
    AutofillClient::PaymentsRpcResult result,
    AutofillClient::PaymentsRpcCardType card_type) {
  PaymentsRpcResult metric_result;
  switch (result) {
    case AutofillClient::PaymentsRpcResult::kSuccess:
      metric_result = PAYMENTS_RESULT_SUCCESS;
      break;
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
      metric_result = PAYMENTS_RESULT_TRY_AGAIN_FAILURE;
      break;
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
      metric_result = PAYMENTS_RESULT_PERMANENT_FAILURE;
      break;
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      metric_result = PAYMENTS_RESULT_NETWORK_ERROR;
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      DCHECK_EQ(card_type, AutofillClient::PaymentsRpcCardType::kVirtualCard);
      metric_result = PAYMENTS_RESULT_VCN_RETRIEVAL_TRY_AGAIN_FAILURE;
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, AutofillClient::PaymentsRpcCardType::kVirtualCard);
      metric_result = PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE;
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      return;
  }

  std::string card_type_suffix;
  switch (card_type) {
    case AutofillClient::PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kUnknown:
      NOTREACHED();
      return;
  }

  base::UmaHistogramEnumeration("Autofill.UnmaskPrompt.GetRealPanResult",
                                metric_result);

  base::UmaHistogramEnumeration(
      "Autofill.UnmaskPrompt.GetRealPanResult." + card_type_suffix,
      metric_result);
}

// static
void AutofillMetrics::LogRealPanDuration(
    const base::TimeDelta& duration,
    AutofillClient::PaymentsRpcResult result,
    AutofillClient::PaymentsRpcCardType card_type) {
  std::string result_suffix;
  std::string card_type_suffix;

  switch (card_type) {
    case AutofillClient::PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kUnknown:
      // Unknown card types imply UnmaskCardRequest::ParseResponse() was never
      // called, due to bad internet connection or otherwise. Log anyway so that
      // we have a rough idea of the magnitude of this problem.
      card_type_suffix = "UnknownCard";
      break;
  }

  switch (result) {
    case AutofillClient::PaymentsRpcResult::kSuccess:
      result_suffix = "Success";
      break;
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
      result_suffix = "Failure";
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, AutofillClient::PaymentsRpcCardType::kVirtualCard);
      result_suffix = "VcnRetrievalFailure";
      break;
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      result_suffix = "NetworkError";
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      return;
  }

  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.GetRealPanDuration",
                              duration);
  base::UmaHistogramLongTimes("Autofill.UnmaskPrompt.GetRealPanDuration." +
                                  card_type_suffix + "." + result_suffix,
                              duration);
}

// static
void AutofillMetrics::LogUnmaskingDuration(
    const base::TimeDelta& duration,
    AutofillClient::PaymentsRpcResult result,
    AutofillClient::PaymentsRpcCardType card_type) {
  std::string result_suffix;
  std::string card_type_suffix;

  switch (card_type) {
    case AutofillClient::PaymentsRpcCardType::kServerCard:
      card_type_suffix = "ServerCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kVirtualCard:
      card_type_suffix = "VirtualCard";
      break;
    case AutofillClient::PaymentsRpcCardType::kUnknown:
      NOTREACHED();
      return;
  }

  switch (result) {
    case AutofillClient::PaymentsRpcResult::kSuccess:
      result_suffix = "Success";
      break;
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
      result_suffix = "Failure";
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      DCHECK_EQ(card_type, AutofillClient::PaymentsRpcCardType::kVirtualCard);
      result_suffix = "VcnRetrievalFailure";
      break;
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      result_suffix = "NetworkError";
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
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
      PREDICTION_SOURCE_HEURISTIC,
      AutofillType(field.heuristic_type()).GetStorableType(),
      form_interactions_ukm_logger, form, field, metric_type,
      false /*log_rationalization_metrics*/);
}

// static
void AutofillMetrics::LogServerPredictionQualityMetrics(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_SERVER,
      AutofillType(field.server_type()).GetStorableType(),
      form_interactions_ukm_logger, form, field, metric_type,
      false /*log_rationalization_metrics*/);
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
      true /*log_rationalization_metrics*/);
}

// static
void AutofillMetrics::LogEditedAutofilledFieldAtSubmissionDeprecated(
    FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field) {
  const std::string aggregate_histogram =
      "Autofill.EditedAutofilledFieldAtSubmission.Aggregate";
  const std::string type_specific_histogram =
      "Autofill.EditedAutofilledFieldAtSubmission.ByFieldType";

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
    bool autocomplete_off = field.autocomplete_attribute == "off";
    const std::string autocomplete_histogram = base::StrCat(
        {"Autofill.Autocomplete.", autocomplete_off ? "Off" : "NotOff",
         ".EditedAutofilledFieldAtSubmission.",
         form_type == FormType::kAddressForm ? "Address" : "CreditCard"});
    base::UmaHistogramEnumeration(autocomplete_histogram, editing_metric);
  }

  // If the field was edited, record the event to UKM.
  // TODO(crbug.com/1368096): Recording of this metric is stopped and moved to
  // the LogEditedAutofilledFieldAtSubmission method.
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
    bool autocomplete_off = field.autocomplete_attribute == "off";
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
void AutofillMetrics::LogUserHappinessMetric(
    UserHappinessMetric metric,
    FieldTypeGroup field_type_group,
    security_state::SecurityLevel security_level,
    uint32_t profile_form_bitmask) {
  LogUserHappinessMetric(metric, {FieldTypeGroupToFormType(field_type_group)},
                         security_level, profile_form_bitmask);
}

// static
void AutofillMetrics::LogUserHappinessMetric(
    UserHappinessMetric metric,
    const DenseSet<FormType>& form_types,
    security_state::SecurityLevel security_level,
    uint32_t profile_form_bitmask) {
  DCHECK_LT(metric, NUM_USER_HAPPINESS_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness", metric,
                            NUM_USER_HAPPINESS_METRICS);
  if (base::Contains(form_types, FormType::kCreditCardForm)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.CreditCard", metric,
                              NUM_USER_HAPPINESS_METRICS);
    LogUserHappinessBySecurityLevel(metric, FormType::kCreditCardForm,
                                    security_level);
  }
  if (base::Contains(form_types, FormType::kAddressForm)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.Address", metric,
                              NUM_USER_HAPPINESS_METRICS);
    if (metric != AutofillMetrics::FORMS_LOADED) {
      LogUserHappinessByProfileFormType(metric, profile_form_bitmask);
    }
    LogUserHappinessBySecurityLevel(metric, FormType::kAddressForm,
                                    security_level);
  }
  if (base::Contains(form_types, FormType::kPasswordForm)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.Password", metric,
                              NUM_USER_HAPPINESS_METRICS);
    LogUserHappinessBySecurityLevel(metric, FormType::kPasswordForm,
                                    security_level);
  }
  if (base::Contains(form_types, FormType::kUnknownFormType)) {
    UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness.Unknown", metric,
                              NUM_USER_HAPPINESS_METRICS);
    LogUserHappinessBySecurityLevel(metric, FormType::kUnknownFormType,
                                    security_level);
  }
}

// static
void AutofillMetrics::LogUserHappinessBySecurityLevel(
    UserHappinessMetric metric,
    FormType form_type,
    security_state::SecurityLevel security_level) {
  if (security_level == security_state::SecurityLevel::SECURITY_LEVEL_COUNT) {
    return;
  }

  std::string histogram_name = "Autofill.UserHappiness.";
  switch (form_type) {
    case FormType::kCreditCardForm:
      histogram_name += "CreditCard";
      break;

    case FormType::kAddressForm:
      histogram_name += "Address";
      break;

    case FormType::kPasswordForm:
      histogram_name += "Password";
      break;

    case FormType::kUnknownFormType:
      histogram_name += "Unknown";
      break;

    default:
      NOTREACHED();
      return;
  }

  base::UmaHistogramEnumeration(security_state::GetSecurityLevelHistogramName(
                                    histogram_name, security_level),
                                metric, NUM_USER_HAPPINESS_METRICS);
}

// static
void AutofillMetrics::LogUserHappinessByProfileFormType(
    UserHappinessMetric metric,
    uint32_t profile_form_bitmask) {
  base::UmaHistogramEnumeration(
      "Autofill.UserHappiness.Address" +
          data_util::GetSuffixForProfileFormType(profile_form_bitmask),
      metric, NUM_USER_HAPPINESS_METRICS);

  if (data_util::ContainsAddress(profile_form_bitmask) &&
      (data_util::ContainsPhone(profile_form_bitmask) ||
       data_util::ContainsEmail(profile_form_bitmask)))
    base::UmaHistogramEnumeration(
        "Autofill.UserHappiness.Address.AddressPlusContact", metric,
        NUM_USER_HAPPINESS_METRICS);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(
    const base::TimeDelta& duration) {
  LogFormFillDuration("Autofill.FillDuration.FromLoad.WithAutofill", duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(
    const base::TimeDelta& duration) {
  LogFormFillDuration("Autofill.FillDuration.FromLoad.WithoutAutofill",
                      duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromLoadForOneTimeCode(
    const base::TimeDelta& duration) {
  LogFormFillDuration("Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad",
                      duration);
}

// static
void AutofillMetrics::LogFormFillDurationFromInteraction(
    const DenseSet<FormType>& form_types,
    bool used_autofill,
    const base::TimeDelta& duration) {
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
    const base::TimeDelta& duration) {
  LogFormFillDuration(
      "Autofill.WebOTP.OneTimeCode.FillDuration.FromInteraction", duration);
}

// static
void AutofillMetrics::LogFormFillDuration(const std::string& metric,
                                          const base::TimeDelta& duration) {
  base::UmaHistogramCustomTimes(metric, duration, base::Milliseconds(100),
                                base::Minutes(10), 50);
}

// static
void AutofillMetrics::LogIsAutofillEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.IsEnabled.Startup", enabled);
}

// static
void AutofillMetrics::LogIsAutofillProfileEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.Address.IsEnabled.Startup", enabled);
}

// static
void AutofillMetrics::LogIsAutofillCreditCardEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.CreditCard.IsEnabled.Startup", enabled);
}

// static
void AutofillMetrics::LogIsAutofillEnabledAtPageLoad(
    bool enabled,
    AutofillSyncSigninState sync_state) {
  std::string name("Autofill.IsEnabled.PageLoad");
  UMA_HISTOGRAM_BOOLEAN(name, enabled);
  base::UmaHistogramBoolean(name + GetMetricsSyncStateSuffix(sync_state),
                            enabled);
}

// static
void AutofillMetrics::LogIsAutofillProfileEnabledAtPageLoad(
    bool enabled,
    AutofillSyncSigninState sync_state) {
  std::string name("Autofill.Address.IsEnabled.PageLoad");
  UMA_HISTOGRAM_BOOLEAN(name, enabled);
  base::UmaHistogramBoolean(name + GetMetricsSyncStateSuffix(sync_state),
                            enabled);
}

// static
void AutofillMetrics::LogIsAutofillCreditCardEnabledAtPageLoad(
    bool enabled,
    AutofillSyncSigninState sync_state) {
  std::string name("Autofill.CreditCard.IsEnabled.PageLoad");
  UMA_HISTOGRAM_BOOLEAN(name, enabled);
  base::UmaHistogramBoolean(name + GetMetricsSyncStateSuffix(sync_state),
                            enabled);
}

// static
void AutofillMetrics::LogStoredCreditCardMetrics(
    const std::vector<std::unique_ptr<CreditCard>>& local_cards,
    const std::vector<std::unique_ptr<CreditCard>>& server_cards,
    size_t server_card_count_with_card_art_image,
    base::TimeDelta disused_data_threshold) {
  size_t num_local_cards = 0;
  size_t num_local_cards_with_nickname = 0;
  size_t num_masked_cards = 0;
  size_t num_masked_cards_with_nickname = 0;
  size_t num_unmasked_cards = 0;
  size_t num_disused_local_cards = 0;
  size_t num_disused_masked_cards = 0;
  size_t num_disused_unmasked_cards = 0;

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
      case CreditCard::LOCAL_CARD:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Local",
            days_since_last_use);
        num_local_cards += 1;
        num_disused_local_cards += disused_delta;
        if (card->HasNonEmptyValidNickname())
          num_local_cards_with_nickname += 1;
        break;
      case CreditCard::MASKED_SERVER_CARD:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server",
            days_since_last_use);
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Masked",
            days_since_last_use);
        num_masked_cards += 1;
        num_disused_masked_cards += disused_delta;
        if (card->HasNonEmptyValidNickname())
          num_masked_cards_with_nickname += 1;
        break;
      case CreditCard::FULL_SERVER_CARD:
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server",
            days_since_last_use);
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Unmasked",
            days_since_last_use);
        num_unmasked_cards += 1;
        num_disused_unmasked_cards += disused_delta;
        break;
      case CreditCard::VIRTUAL_CARD:
        // This card type is not persisted in Chrome.
        NOTREACHED();
        break;
    }
  }

  // Calculate some summary info.
  const size_t num_server_cards = num_masked_cards + num_unmasked_cards;
  const size_t num_cards = num_local_cards + num_server_cards;
  const size_t num_disused_server_cards =
      num_disused_masked_cards + num_disused_unmasked_cards;
  const size_t num_disused_cards =
      num_disused_local_cards + num_disused_server_cards;

  // Log the overall counts.
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount", num_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Local",
                            num_local_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Local.WithNickname",
                            num_local_cards_with_nickname);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Server",
                            num_server_cards);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Server.Masked",
                            num_masked_cards);
  UMA_HISTOGRAM_COUNTS_1000(
      "Autofill.StoredCreditCardCount.Server.Masked.WithNickname",
      num_masked_cards_with_nickname);
  UMA_HISTOGRAM_COUNTS_1000("Autofill.StoredCreditCardCount.Server.Unmasked",
                            num_unmasked_cards);

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
  if (num_masked_cards) {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.StoredCreditCardDisusedCount.Server.Masked",
        num_disused_masked_cards);
  }
  if (num_unmasked_cards) {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.StoredCreditCardDisusedCount.Server.Unmasked",
        num_disused_unmasked_cards);
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
void AutofillMetrics::LogHiddenOrPresentationalSelectFieldsFilled() {
  UMA_HISTOGRAM_BOOLEAN("Autofill.HiddenOrPresentationalSelectFieldsFilled",
                        true);
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
void AutofillMetrics::LogNumberOfAddressesDeletedForDisuse(
    size_t num_profiles) {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.AddressesDeletedForDisuse", num_profiles);
}

// static
void AutofillMetrics::LogAddressSuggestionsCount(size_t num_suggestions) {
  UMA_HISTOGRAM_COUNTS_1M("Autofill.AddressSuggestionsCount", num_suggestions);
}

// static
void AutofillMetrics::LogAutofillPopupHidingReason(PopupHidingReason reason) {
  base::UmaHistogramEnumeration("Autofill.PopupHidingReason", reason);
}

// static
void AutofillMetrics::LogAutofillFormCleared() {
  base::RecordAction(base::UserMetricsAction("Autofill_ClearedForm"));
}

// static
void AutofillMetrics::LogNumberOfEditedAutofilledFields(
    size_t num_edited_autofilled_fields,
    bool observed_submission) {
  if (observed_submission) {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.NumberOfEditedAutofilledFieldsAtSubmission",
        num_edited_autofilled_fields);
  } else {
    UMA_HISTOGRAM_COUNTS_1000(
        "Autofill.NumberOfEditedAutofilledFieldsAtSubmission.NoSubmission",
        num_edited_autofilled_fields);
  }
}

// static
void AutofillMetrics::LogFieldFillingStats(
    FormType form_type,
    const FormGroupFillingStats& filling_stats) {
  std::string histogram_prefix = base::StrCat(
      {"Autofill.FieldFillingStats.", FormTypeToStringPiece(form_type), "."});

  // Do not acquire metrics if autofill was not used in this form group.
  if (filling_stats.TotalFilled() == 0)
    return;

  // Counts into those histograms are mutually exclusive.
  base::UmaHistogramCounts100(base::StrCat({histogram_prefix, "Accepted"}),
                              filling_stats.num_accepted);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "CorrectedToSameType"}),
      filling_stats.num_corrected_to_same_type);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "CorrectedToDifferentType"}),
      filling_stats.num_corrected_to_different_type);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "CorrectedToUnknownType"}),
      filling_stats.num_corrected_to_unknown_type);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "CorrectedToEmpty"}),
      filling_stats.num_corrected_to_empty);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "ManuallyFilledToSameType"}),
      filling_stats.num_manually_filled_to_same_type);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "ManuallyFilledToDifferentType"}),
      filling_stats.num_manually_filled_to_differt_type);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "ManuallyFilledToUnknownType"}),
      filling_stats.num_manually_filled_to_unknown_type);

  base::UmaHistogramCounts100(base::StrCat({histogram_prefix, "LeftEmpty"}),
                              filling_stats.num_left_empty);

  // Counts into those histograms are not mutually exclusive and a single field
  // can contribute to multiple of those.
  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "TotalCorrected"}),
      filling_stats.TotalCorrected());

  base::UmaHistogramCounts100(base::StrCat({histogram_prefix, "TotalFilled"}),
                              filling_stats.TotalFilled());

  base::UmaHistogramCounts100(base::StrCat({histogram_prefix, "TotalUnfilled"}),
                              filling_stats.TotalUnfilled());

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "TotalManuallyFilled"}),
      filling_stats.TotalManuallyFilled());

  base::UmaHistogramCounts100(base::StrCat({histogram_prefix, "Total"}),
                              filling_stats.Total());
}

void AutofillMetrics::LogSectioningMetrics(
    const base::flat_map<Section, size_t>& fields_per_section) {
  constexpr base::StringPiece kBaseHistogramName = "Autofill.Sectioning.";
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
void AutofillMetrics::LogProfileActionOnFormSubmitted(
    AutofillProfileAction action) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.ProfileActionOnFormSubmitted", action,
                            AUTOFILL_PROFILE_ACTION_ENUM_SIZE);
}

// static
void AutofillMetrics::LogAutofillFormSubmittedState(
    AutofillFormSubmittedState state,
    bool is_for_credit_card,
    bool has_upi_vpa_field,
    const DenseSet<FormType>& form_types,
    const base::TimeTicks& form_parsed_timestamp,
    FormSignature form_signature,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormInteractionCounts& form_interaction_counts) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.FormSubmittedState", state,
                            AUTOFILL_FORM_SUBMITTED_STATE_ENUM_SIZE);

  switch (state) {
    case NON_FILLABLE_FORM_OR_NEW_DATA:
      base::RecordAction(
          base::UserMetricsAction("Autofill_FormSubmitted_NonFillable"));
      break;

    case FILLABLE_FORM_AUTOFILLED_ALL:
      base::RecordAction(
          base::UserMetricsAction("Autofill_FormSubmitted_FilledAll"));
      break;

    case FILLABLE_FORM_AUTOFILLED_SOME:
      base::RecordAction(
          base::UserMetricsAction("Autofill_FormSubmitted_FilledSome"));
      break;

    case FILLABLE_FORM_AUTOFILLED_NONE_DID_SHOW_SUGGESTIONS:
      base::RecordAction(base::UserMetricsAction(
          "Autofill_FormSubmitted_FilledNone_SuggestionsShown"));
      break;

    case FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS:
      base::RecordAction(base::UserMetricsAction(
          "Autofill_FormSubmitted_FilledNone_SuggestionsNotShown"));
      break;

    default:
      NOTREACHED();
      break;
  }
  form_interactions_ukm_logger->LogFormSubmitted(
      is_for_credit_card, has_upi_vpa_field, form_types, state,
      form_parsed_timestamp, form_signature, form_interaction_counts);
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
    const ServerFieldTypeSet& filled_types)
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
  NOTREACHED();
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
  NOTREACHED();
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
    ServerFieldTypeSet autofilled_types;
    for (const auto& field : *p.form) {
      FieldGlobalId id = field->global_id();
      if (only_newly_filled_fields && !p.newly_filled_fields->contains(id))
        continue;
      if (only_after_security_policy && !p.safe_fields->contains(id))
        continue;
      if (only_visible_fields && !field->is_visible)
        continue;
      autofilled_types.insert(field->Type().GetStorableType());
    }
    return CreditCardSeamlessness(autofilled_types);
  };

  auto RecordUma = [](base::StringPiece infix, CreditCardSeamlessness s) {
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
    auto IsSensitiveFieldType = [](ServerFieldType field_type) {
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
    const url::Origin& triggered_origin = p.field->origin;
    return field.origin != triggered_origin &&
           (field.origin != main_origin ||
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
    const ServerFieldTypeSet& autofilled_types) {
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
void AutofillMetrics::LogParseFormTiming(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("Autofill.Timing.ParseForm", duration);
}

// static
void AutofillMetrics::LogNumberOfProfilesConsideredForDedupe(
    size_t num_considered) {
  // A maximum of 50 is enforced to reduce the number of generated buckets.
  UMA_HISTOGRAM_COUNTS_1000(
      "Autofill.NumberOfProfilesConsideredForDedupe",
      std::min(static_cast<int>(num_considered), kMaxBucketsCount));
}

// static
void AutofillMetrics::LogNumberOfProfilesRemovedDuringDedupe(
    size_t num_removed) {
  // A maximum of 50 is enforced to reduce the number of generated buckets.
  UMA_HISTOGRAM_COUNTS_1000(
      "Autofill.NumberOfProfilesRemovedDuringDedupe",
      std::min(static_cast<int>(num_removed), kMaxBucketsCount));
}

// static
void AutofillMetrics::LogIsQueriedCreditCardFormSecure(bool is_secure) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.QueriedCreditCardFormIsSecure", is_secure);
}

// static
void AutofillMetrics::LogWalletAddressConversionType(
    WalletAddressConversionType type) {
  DCHECK_LT(type, NUM_CONVERTED_ADDRESS_CONVERSION_TYPES);
  UMA_HISTOGRAM_ENUMERATION("Autofill.WalletAddressConversionType", type,
                            NUM_CONVERTED_ADDRESS_CONVERSION_TYPES);
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
void AutofillMetrics::LogAutocompleteSuggestionAcceptedIndex(int index) {
  base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Autocomplete",
                           std::min(index, kMaxBucketsCount));
  AutofillMetrics::Log(AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_SELECTED);
}

// static
void AutofillMetrics::OnAutocompleteSuggestionsShown() {
  AutofillMetrics::Log(AutocompleteEvent::AUTOCOMPLETE_SUGGESTIONS_SHOWN);
}

// static
void AutofillMetrics::OnAutocompleteSuggestionDeleted(
    AutocompleteSingleEntryRemovalMethod removal_method) {
  AutofillMetrics::Log(AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED);
  base::UmaHistogramEnumeration(
      "Autofill.Autocomplete.SingleEntryRemovalMethod", removal_method);
}

// static
void AutofillMetrics::Log(AutocompleteEvent event) {
  DCHECK_LT(event, AutocompleteEvent::NUM_AUTOCOMPLETE_EVENTS);
  base::UmaHistogramEnumeration("Autocomplete.Events", event,
                                NUM_AUTOCOMPLETE_EVENTS);
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
    case SubmissionSource::DOM_MUTATION_AFTER_XHR:
      return "Autofill.UploadEvent.DomMutationAfterXhr";
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return "Autofill.UploadEvent.ProbablyFormSubmitted";
    case SubmissionSource::FORM_SUBMISSION:
      return "Autofill.UploadEvent.FormSubmission";
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
    DenseSet<FormType> form_types,
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
    ukm::UkmRecorder* ukm_recorder,
    const ukm::SourceId source_id)
    : ukm_recorder_(ukm_recorder), source_id_(source_id) {}

ukm::builders::Autofill_CreditCardFill
AutofillMetrics::FormInteractionsUkmLogger::CreateCreditCardFillBuilder()
    const {
  return ukm::builders::Autofill_CreditCardFill(source_id_);
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

  ukm::builders::Autofill_InteractedWithForm(source_id_)
      .SetIsForCreditCard(is_for_credit_card)
      .SetLocalRecordTypeCount(local_record_type_count)
      .SetServerRecordTypeCount(server_record_type_count)
      .SetFormSignature(HashFormSignature(form_signature))
      .Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogSuggestionsShown(
    const FormStructure& form,
    const AutofillField& field,
    const base::TimeTicks& form_parsed_timestamp,
    bool off_the_record) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_SuggestionsShown(source_id_)
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
    absl::variant<AutofillProfile::RecordType, CreditCard::RecordType>
        record_type,
    const FormStructure& form,
    const AutofillField& field) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_SuggestionFilled(source_id_)
      .SetRecordType(absl::visit(
          [](auto value) { return base::to_underlying(value); }, record_type))
      .SetIsForCreditCard(
          absl::holds_alternative<CreditCard::RecordType>(record_type))
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

  ukm::builders::Autofill_EditedAutofilledFieldAtSubmission(source_id_)
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

  ukm::builders::Autofill_TextFieldDidChange(source_id_)
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFieldTypeGroup(static_cast<int>(field.Type().group()))
      .SetHeuristicType(static_cast<int>(field.heuristic_type()))
      .SetServerType(static_cast<int>(field.server_type()))
      .SetHtmlFieldType(static_cast<int>(field.html_type()))
      .SetHtmlFieldMode(static_cast<int>(field.html_mode()))
      .SetIsAutofilled(field.is_autofilled)
      .SetIsEmpty(field.IsEmpty())
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

  ukm::builders::Autofill_FieldFillStatus(source_id_)
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form.form_parsed_timestamp()))
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetValidationEvent(static_cast<int64_t>(metric_type))
      .SetIsAutofilled(static_cast<int64_t>(field.is_autofilled))
      .SetWasPreviouslyAutofilled(
          static_cast<int64_t>(field.previously_autofilled()))
      .Record(ukm_recorder_);
}

// TODO(szhangcs): Take FormStructure and AutofillField and extract
// FormSignature and TimeTicks inside the function.
void AutofillMetrics::FormInteractionsUkmLogger::LogFieldType(
    const base::TimeTicks& form_parsed_timestamp,
    FormSignature form_signature,
    FieldSignature field_signature,
    QualityMetricPredictionSource prediction_source,
    QualityMetricType metric_type,
    ServerFieldType predicted_type,
    ServerFieldType actual_type) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_FieldTypeValidation(source_id_)
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
  OptionalBoolean was_focused = OptionalBoolean::kFalse;
  OptionalBoolean suggestion_was_available = OptionalBoolean::kUndefined;
  OptionalBoolean suggestion_was_shown = OptionalBoolean::kUndefined;
  OptionalBoolean suggestion_was_accepted = OptionalBoolean::kUndefined;

  OptionalBoolean was_autofilled = OptionalBoolean::kUndefined;
  OptionalBoolean had_value_before_filling = OptionalBoolean::kUndefined;
  DenseSet<SkipStatus> autofill_skipped_status;
  size_t autofill_count = 0;

  OptionalBoolean user_typed_into_field = OptionalBoolean::kFalse;
  OptionalBoolean filled_value_was_modified = OptionalBoolean::kUndefined;
  OptionalBoolean had_typed_or_filled_value_at_submission =
      OptionalBoolean::kUndefined;
  OptionalBoolean had_value_after_filling = OptionalBoolean::kUndefined;
  OptionalBoolean has_value_after_typing = OptionalBoolean::kUndefined;

  // TODO(crbug.com/1325851): Add a metric in |FieldInfo| UKM event to indicate
  // whether the user had any data available for the respective field type.

  // If multiple fields have the same signature, this indicates the position
  // within this set of fields. This allows us to understand problems related
  // to duplicated field signatures.
  size_t rank_in_field_signature_group = 0;

  // Field types from local heuristics prediction.
  // The field type from the active local heuristic pattern.
  ServerFieldType heuristic_type = UNKNOWN_TYPE;
  // The type of the field predicted from patterns whose stability is above
  // suspicion.
  ServerFieldType heuristic_legacy_type = UNKNOWN_TYPE;
  // The type of the field predicted from the source of local heuristics on
  // the client, which uses patterns applied for most users.
  ServerFieldType heuristic_default_type = UNKNOWN_TYPE;
  // The type of the field predicted from the heuristics that uses experimental
  // patterns.
  ServerFieldType heuristic_experimental_type = UNKNOWN_TYPE;
  // The type of the field predicted from the heuristics that uses patterns
  // only for non-user-visible metrics, one step before experimental.
  ServerFieldType heuristic_next_gen_type = UNKNOWN_TYPE;

  // Field types from Autocomplete attribute.
  // Information of the HTML autocomplete attribute, see
  // components/autofill/core/common/mojom/autofill_types.mojom.
  HtmlFieldMode html_mode = HtmlFieldMode::kNone;
  HtmlFieldType html_type = HtmlFieldType::kUnrecognized;

  // The field type predicted by the Autofill crowdsourced server from
  // majority voting.
  ServerFieldType server_type1 = NO_SERVER_DATA;
  FieldPrediction::Source prediction_source1 =
      FieldPrediction::SOURCE_UNSPECIFIED;
  ServerFieldType server_type2 = NO_SERVER_DATA;
  FieldPrediction::Source prediction_source2 =
      FieldPrediction::SOURCE_UNSPECIFIED;
  // This is an annotation for server predicted field types which indicates
  // that a manual override defines the server type.
  bool server_type_is_override = false;

  // The final field type from the list of |autofill::ServerFieldType| that we
  // choose after rationalization, which is used to determine
  // the autofill suggestion when the user triggers autofilling.
  ServerFieldType overall_type = NO_SERVER_DATA;
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
    static_assert(absl::variant_size<AutofillField::FieldLogEventType>() == 9,
                  "When adding new variants check that this function does not "
                  "need to be updated.");
    if (auto* event =
            absl::get_if<AskForValuesToFillFieldLogEvent>(&log_event)) {
      was_focused = OptionalBoolean::kTrue;
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
      was_autofilled |= event->was_autofilled;
      had_value_before_filling |= event->had_value_before_filling;
      autofill_skipped_status.insert(event->autofill_skipped_status);
      had_value_after_filling = event->had_value_after_filling;
      if (was_autofilled == OptionalBoolean::kTrue &&
          filled_value_was_modified == OptionalBoolean::kUndefined) {
        // Initialize filled_value_was_modified to a defined value when the
        // field is filled for the first time.
        filled_value_was_modified = OptionalBoolean::kFalse;
      }
      ++autofill_count;
    }

    if (auto* event = absl::get_if<TypingFieldLogEvent>(&log_event)) {
      user_typed_into_field = OptionalBoolean::kTrue;
      if (was_autofilled == OptionalBoolean::kTrue) {
        filled_value_was_modified = OptionalBoolean::kTrue;
      }
      has_value_after_typing = event->has_value_after_typing;
    }

    if (auto* event =
            absl::get_if<HeuristicPredictionFieldLogEvent>(&log_event)) {
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
      switch (event->pattern_source) {
        case PatternSource::kLegacy:
          heuristic_legacy_type = event->field_type;
          break;
        case PatternSource::kDefault:
          heuristic_default_type = event->field_type;
          break;
        case PatternSource::kExperimental:
          heuristic_experimental_type = event->field_type;
          break;
        case PatternSource::kNextGen:
          heuristic_next_gen_type = event->field_type;
          break;
      }
#else
      switch (event->pattern_source) {
        case PatternSource::kLegacy:
          heuristic_legacy_type = event->field_type;
          break;
      }
#endif

      if (event->is_active_pattern_source) {
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

  ukm::builders::Autofill2_FieldInfo builder(source_id_);
  builder
      .SetFormSessionIdentifier(
          AutofillMetrics::FormGlobalIdToHash64Bit(form.global_id()))
      .SetFieldSessionIdentifier(
          AutofillMetrics::FieldGlobalIdToHash64Bit(field.global_id()))
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFormControlType(base::to_underlying(field.FormControlType()))
      .SetAutocompleteState(base::to_underlying(autocomplete_state));

  SetStatusVector(AutofillStatus::kIsFocusable, field.IsFocusable());
  SetStatusVector(AutofillStatus::kUserTypedIntoField,
                  OptionalBooleanToBool(user_typed_into_field));
  SetStatusVector(AutofillStatus::kWasFocused,
                  OptionalBooleanToBool(was_focused));
  SetStatusVector(AutofillStatus::kIsInSubFrame,
                  form.ToFormData().host_frame != field.host_frame);

  if (was_focused == OptionalBoolean::kTrue) {
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
    SetStatusVector(AutofillStatus::kWasAutofilled,
                    OptionalBooleanToBool(was_autofilled));
    SetStatusVector(AutofillStatus::kHadValueBeforeFilling,
                    OptionalBooleanToBool(had_value_before_filling));
    SetStatusVector(AutofillStatus::kWasRefill, autofill_count > 1);

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
        .SetHeuristicTypeExperimental(heuristic_experimental_type)
        .SetHeuristicTypeNextGen(heuristic_next_gen_type);
  }

  if (had_html_type) {
    builder.SetHtmlFieldType(base::to_underlying(html_type))
        .SetHtmlFieldMode(base::to_underlying(html_mode));
  }

  if (had_server_type) {
    builder.SetServerType1(server_type1)
        .SetServerPredictionSource1(prediction_source1)
        .SetServerType2(server_type2)
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

void AutofillMetrics::FormInteractionsUkmLogger::
    LogAutofillFormSummaryAtFormRemove(
        const FormStructure& form_structure,
        FormEventSet form_events,
        const base::TimeTicks& initial_interaction_timestamp,
        const base::TimeTicks& form_submitted_timestamp) {
  if (!CanLog()) {
    return;
  }

  static_assert(form_events.data().size() == 2U,
                "If you add a new form event, you need to create a new "
                "AutofillFormEvents metric in Autofill2.FormSummary");
  ukm::builders::Autofill2_FormSummary builder(source_id_);
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
        ukm::GetSemanticBucketMinForDurationTiming(
            (form_submitted_timestamp - form_structure.form_parsed_timestamp())
                .InMilliseconds()));
  }

  if (!form_submitted_timestamp.is_null() &&
      !initial_interaction_timestamp.is_null() &&
      form_submitted_timestamp > initial_interaction_timestamp) {
    builder.SetMillisecondsFromFirstInteratctionUntilSubmission(
        ukm::GetSemanticBucketMinForDurationTiming(
            (form_submitted_timestamp - initial_interaction_timestamp)
                .InMilliseconds()));
  }
  builder.Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::
    LogHiddenRepresentationalFieldSkipDecision(const FormStructure& form,
                                               const AutofillField& field,
                                               bool is_skipped) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_HiddenRepresentationalFieldSkipDecision(source_id_)
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
        ServerFieldType old_type) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_RepeatedServerTypePredictionRationalized(source_id_)
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
  ukm::builders::Autofill_Sectioning(source_id_)
      .SetFormSignature(HashFormSignature(form_signature))
      .SetSectioningSignature(sectioning_signature % 1024)
      .Record(ukm_recorder_);
}

int64_t AutofillMetrics::FormTypesToBitVector(
    const DenseSet<FormType>& form_types) {
  int64_t form_type_bv = 0;
  for (FormType form_type : form_types) {
    DCHECK_LT(static_cast<int64_t>(form_type), 63);
    form_type_bv |= 1LL << static_cast<int64_t>(form_type);
  }
  return form_type_bv;
}

void AutofillMetrics::LogServerCardLinkClicked(
    AutofillSyncSigninState sync_state) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.ServerCardLinkClicked", sync_state,
                            AutofillSyncSigninState::kNumSyncStates);
}

// static
const char* AutofillMetrics::GetMetricsSyncStateSuffix(
    AutofillSyncSigninState sync_state) {
  switch (sync_state) {
    case AutofillSyncSigninState::kSignedOut:
      return ".SignedOut";
    case AutofillSyncSigninState::kSignedIn:
      return ".SignedIn";
    case AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled:
      return ".SignedInAndWalletSyncTransportEnabled";
    case AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled:
      return ".SignedInAndSyncFeatureEnabled";
    case AutofillSyncSigninState::kSyncPaused:
      return ".SyncPaused";
    case AutofillSyncSigninState::kNumSyncStates:
      return ".Unknown";
  }
}

void AutofillMetrics::FormInteractionsUkmLogger::LogFormSubmitted(
    bool is_for_credit_card,
    bool has_upi_vpa_field,
    const DenseSet<FormType>& form_types,
    AutofillFormSubmittedState state,
    const base::TimeTicks& form_parsed_timestamp,
    FormSignature form_signature,
    const FormInteractionCounts& form_interaction_counts) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_FormSubmitted builder(source_id_);
  builder.SetAutofillFormSubmittedState(static_cast<int>(state))
      .SetIsForCreditCard(is_for_credit_card)
      .SetHasUpiVpaField(has_upi_vpa_field)
      .SetFormTypes(FormTypesToBitVector(form_types))
      .SetFormSignature(HashFormSignature(form_signature))
      .SetFormElementUserModifications(
          std::min(form_interaction_counts.form_element_user_modifications,
                   kFormUserInteractionsOverflowBucket))
      .SetAutofillFills(std::min(form_interaction_counts.autofill_fills,
                                 kFormUserInteractionsOverflowBucket));
  if (form_parsed_timestamp.is_null())
    DCHECK(state == NON_FILLABLE_FORM_OR_NEW_DATA ||
           state == FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS)
        << state;
  else
    builder.SetMillisecondsSinceFormParsed(
        MillisecondsSinceFormParsed(form_parsed_timestamp));

  builder.Record(ukm_recorder_);
}

void AutofillMetrics::FormInteractionsUkmLogger::LogKeyMetrics(
    const DenseSet<FormType>& form_types,
    bool data_to_fill_available,
    bool suggestions_shown,
    bool edited_autofilled_field,
    bool suggestion_filled,
    const FormInteractionCounts& form_interaction_counts,
    const FormInteractionsFlowId& flow_id,
    absl::optional<int64_t> fast_checkout_run_id) {
  if (!CanLog())
    return;

  ukm::builders::Autofill_KeyMetrics builder(source_id_);
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
    const DenseSet<FormType>& form_types,
    const base::TimeTicks& form_parsed_timestamp) {
  if (!CanLog())
    return;

  if (form_parsed_timestamp.is_null())
    return;

  ukm::builders::Autofill_FormEvent builder(source_id_);
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
    const base::TimeTicks& form_parsed_timestamp) const {
  DCHECK(!form_parsed_timestamp.is_null());
  // Use the pinned timestamp as the current time if it's set.
  base::TimeTicks now = pinned_timestamp_.is_null()
                            ? AutofillTickClock::NowTicks()
                            : pinned_timestamp_;

  return ukm::GetExponentialBucketMin(
      (now - form_parsed_timestamp).InMilliseconds(),
      kAutofillEventDataBucketSpacing);
}

AutofillMetrics::UkmTimestampPin::UkmTimestampPin(
    FormInteractionsUkmLogger* logger)
    : logger_(logger) {
  DCHECK(logger_);
  DCHECK(!logger_->has_pinned_timestamp());
  logger_->set_pinned_timestamp(AutofillTickClock::NowTicks());
}

AutofillMetrics::UkmTimestampPin::~UkmTimestampPin() {
  DCHECK(logger_->has_pinned_timestamp());
  logger_->set_pinned_timestamp(base::TimeTicks());
}

// static
void AutofillMetrics::LogFieldParsingPageTranslationStatusMetric(bool metric) {
  base::UmaHistogramBoolean("Autofill.ParsedFieldTypesWasPageTranslated",
                            metric);
}

// static
void AutofillMetrics::LogFieldParsingTranslatedFormLanguageMetric(
    base::StringPiece locale) {
  base::UmaHistogramSparse(
      "Autofill.ParsedFieldTypesUsingTranslatedPageLanguage",
      language::LanguageUsageMetrics::ToLanguageCodeHash(locale));
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

// static
void AutofillMetrics::LogNumberOfAutofilledFieldsAtSubmission(
    size_t number_of_accepted_fields,
    size_t number_of_corrected_fields) {
  base::UmaHistogramExactLinear(
      "Autofill.NumberOfAutofilledFieldsAtSubmission.Total",
      number_of_accepted_fields + number_of_corrected_fields, 50);
  base::UmaHistogramExactLinear(
      "Autofill.NumberOfAutofilledFieldsAtSubmission.Accepted",
      number_of_accepted_fields, 50);
  base::UmaHistogramExactLinear(
      "Autofill.NumberOfAutofilledFieldsAtSubmission.Corrected",
      number_of_corrected_fields, 50);
}

// static
void AutofillMetrics::
    LogNumberOfAutofilledFieldsWithAutocompleteUnrecognizedAtSubmission(
        size_t number_of_accepted_fields,
        size_t number_of_corrected_fields) {
  base::UmaHistogramExactLinear(
      "Autofill."
      "NumberOfAutofilledFieldsWithAutocompleteUnrecognizedAtSubmission.Total",
      number_of_accepted_fields + number_of_corrected_fields, 50);
  base::UmaHistogramExactLinear(
      "Autofill."
      "NumberOfAutofilledFieldsWithAutocompleteUnrecognizedAtSubmission."
      "Accepted",
      number_of_accepted_fields, 50);
  base::UmaHistogramExactLinear(
      "Autofill."
      "NumberOfAutofilledFieldsWithAutocompleteUnrecognizedAtSubmission."
      "Corrected",
      number_of_corrected_fields, 50);
}

// static
void AutofillMetrics::LogPhoneNumberGrammarMatched(int grammar_id,
                                                   bool suffix_matched,
                                                   int num_grammars) {
  DCHECK(0 <= grammar_id && grammar_id < num_grammars);
  int metric = 2 * grammar_id + suffix_matched;
  int max_metric = 2 * (num_grammars - 1) + 1;
  // Add 1 everywhere, because UmaHistogramExactLinear is 1-based.
  base::UmaHistogramExactLinear(
      "Autofill.FieldPrediction.PhoneNumberGrammarUsage", metric + 1,
      /*exclusive_max=*/max_metric + 2);
}

void AutofillMetrics::LogVerificationStatusOfNameTokensOnProfileUsage(
    const AutofillProfile& profile) {
  constexpr base::StringPiece base_histogram_name =
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
  constexpr base::StringPiece base_histogram_name =
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
void AutofillMetrics::LogImageFetcherRequestLatency(
    const base::TimeDelta& duration) {
  base::UmaHistogramLongTimes("Autofill.ImageFetcher.RequestLatency", duration);
}

// static
void AutofillMetrics::
    LogIsValueNotAutofilledOverExistingValueSameAsSubmittedValue(bool is_same) {
  base::UmaHistogramBoolean(
      "Autofill.IsValueNotAutofilledOverExistingValueSameAsSubmittedValue2",
      is_same);
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
    ServerFieldType server_type,
    ServerFieldType heuristic_type) {
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
    default:
      NOTREACHED();
  }

  // Log the metric for heuristic and server type.
  std::string kHistogramName =
      "Autofill.Autocomplete.PredictionCollisionType2.";
  if (server_type != NO_SERVER_DATA) {
    base::UmaHistogramEnumeration(
        kHistogramName + "Server." + autocomplete_suffix, server_type,
        ServerFieldType::MAX_VALID_FIELD_TYPE);
  }
  base::UmaHistogramEnumeration(
      kHistogramName + "Heuristics." + autocomplete_suffix, heuristic_type,
      ServerFieldType::MAX_VALID_FIELD_TYPE);
  base::UmaHistogramEnumeration(
      kHistogramName + "ServerOrHeuristics." + autocomplete_suffix,
      server_type != NO_SERVER_DATA ? server_type : heuristic_type,
      ServerFieldType::MAX_VALID_FIELD_TYPE);
}

// static
void AutofillMetrics::LogContextMenuImpressionsForField(
    ServerFieldType field_type,
    AutocompleteState autocomplete_state) {
  base::UmaHistogramEnumeration(
      "Autofill.FieldContextMenuImpressions.ByAutocomplete",
      autocomplete_state);
  base::UmaHistogramSparse(
      "Autofill.FieldContextMenuImpressions.ByAutofillType", field_type);
}

// static
void AutofillMetrics::LogContextMenuImpressionsForForm(
    int num_of_fields_with_context_menu_shown) {
  base::UmaHistogramSparse(
      "Autofill.FormContextMenuImpressions.ByNumberOfFields",
      base::ranges::clamp(num_of_fields_with_context_menu_shown, 0,
                          kMaxBucketsCount));
}

const std::string PaymentsRpcResultToMetricsSuffix(
    AutofillClient::PaymentsRpcResult result) {
  std::string result_suffix;

  switch (result) {
    case AutofillClient::PaymentsRpcResult::kSuccess:
      result_suffix = ".Success";
      break;
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
      result_suffix = ".Failure";
      break;
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      result_suffix = ".NetworkError";
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      result_suffix = ".VcnRetrievalFailure";
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
  }

  return result_suffix;
}

// static
void AutofillMetrics::LogNumericQuantityCollidesWithServerPrediction(
    bool collision) {
  base::UmaHistogramBoolean(
      "Autofill.NumericQuantityCollidesWithServerPrediction", collision);
}

// static
void AutofillMetrics::
    LogAcceptedFilledFieldWithNumericQuantityHeuristicPrediction(
        bool accepted) {
  base::UmaHistogramBoolean(
      "Autofill.AcceptedFilledFieldWithNumericQuantityHeuristicPrediction",
      accepted);
}

// static
std::string AutofillMetrics::GetHistogramStringForCardType(
    absl::variant<AutofillClient::PaymentsRpcCardType, CreditCard::RecordType>
        card_type) {
  if (absl::holds_alternative<AutofillClient::PaymentsRpcCardType>(card_type)) {
    switch (absl::get<AutofillClient::PaymentsRpcCardType>(card_type)) {
      case AutofillClient::PaymentsRpcCardType::kServerCard:
        return ".ServerCard";
      case AutofillClient::PaymentsRpcCardType::kVirtualCard:
        return ".VirtualCard";
      case AutofillClient::PaymentsRpcCardType::kUnknown:
        NOTREACHED();
        break;
    }
  } else if (absl::holds_alternative<CreditCard::RecordType>(card_type)) {
    switch (absl::get<CreditCard::RecordType>(card_type)) {
      case CreditCard::FULL_SERVER_CARD:
      case CreditCard::MASKED_SERVER_CARD:
        return ".ServerCard";
      case CreditCard::VIRTUAL_CARD:
        return ".VirtualCard";
      case CreditCard::LOCAL_CARD:
        return ".LocalCard";
    }
  }

  return "";
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

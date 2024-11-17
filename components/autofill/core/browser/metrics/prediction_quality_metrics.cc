// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/prediction_quality_metrics.h"

#include <string>
#include <string_view>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/label_source_util.h"

namespace autofill::autofill_metrics {

namespace {

constexpr std::string_view kFieldPredictionMetricPrefix =
    "Autofill.FieldPrediction.";
constexpr std::string_view kAggregateFieldPredictionMetricPrefix =
    "Autofill.FieldPredictionQuality.Aggregate.";
constexpr std::string_view kByFieldTypeFieldPredictionMetricPrefix =
    "Autofill.FieldPredictionQuality.ByFieldType.";

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
int GetFieldTypeGroupPredictionQualityMetric(FieldType field_type,
                                             FieldTypeQualityMetric metric) {
  DCHECK_LT(metric, NUM_FIELD_TYPE_QUALITY_METRICS);

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
        case ALTERNATIVE_FULL_NAME:
        case ALTERNATIVE_GIVEN_NAME:
        case ALTERNATIVE_FAMILY_NAME:
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
          NOTREACHED() << field_type << " type is not in that group.";
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
          NOTREACHED() << field_type << " has no group assigned (ambiguous)";
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
      NOTREACHED();
  }

  // Use bits 8-15 for the group and bits 0-7 for the metric.
  static_assert(NUM_FIELD_TYPE_QUALITY_METRICS <= UINT8_MAX,
                "maximum field type quality metric must fit into 8 bits");
  static_assert(NUM_FIELD_TYPE_GROUPS_FOR_METRICS <= UINT8_MAX,
                "number of field type groups must fit into 8 bits");
  return (group << 8) | metric;
}

namespace {

const char* GetQualityMetricPredictionSource(
    QualityMetricPredictionSource source) {
  switch (source) {
    case PREDICTION_SOURCE_UNKNOWN:
      NOTREACHED();
    case PREDICTION_SOURCE_HEURISTIC:
      return "Heuristic";
    case PREDICTION_SOURCE_SERVER:
      return "Server";
    case PREDICTION_SOURCE_OVERALL:
      return "Overall";
    case PREDICTION_SOURCE_ML_PREDICTIONS:
      return "ML";
  }
}

const char* GetQualityMetricTypeSuffix(QualityMetricType metric_type) {
  switch (metric_type) {
    default:
      NOTREACHED();
    case TYPE_SUBMISSION:
      return "";
    case TYPE_NO_SUBMISSION:
      return ".NoSubmission";
    case TYPE_AUTOCOMPLETE_BASED:
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

  if (possible_types.count(predicted_type)) {
    return predicted_type;
  }

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
  if (collapsed_field_types.size() == 1) {
    actual_type = *collapsed_field_types.begin();
  }

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
    base::UmaHistogramSparse(
        aggregate_histogram,
        (is_empty ? TRUE_NEGATIVE_EMPTY
                  : (is_ambiguous ? TRUE_NEGATIVE_AMBIGUOUS
                                  : TRUE_NEGATIVE_UNKNOWN)));
    if (log_rationalization_metrics) {
      base::UmaHistogramSparse(
          kRationalizationQualityHistogram,
          (is_empty ? RATIONALIZATION_GOOD : RATIONALIZATION_OK));
    }
    return;
  }

  // If it is filled with same type as predicted, it is a true positive. We
  // also log an RATIONALIZATION_BAD by checking if the filled value is filled
  // already in previous fields, this means autofill could have filled it
  // automatically if there has been no rationalization.
  if (predicted_type == actual_type) {
    base::UmaHistogramSparse(aggregate_histogram, TRUE_POSITIVE);
    base::UmaHistogramSparse(
        type_specific_histogram,
        GetFieldTypeGroupPredictionQualityMetric(actual_type, TRUE_POSITIVE));
    if (log_rationalization_metrics) {
      bool duplicated_filling = DuplicatedFilling(form, field);
      base::UmaHistogramSparse(
          kRationalizationQualityHistogram,
          (duplicated_filling ? RATIONALIZATION_BAD : RATIONALIZATION_OK));
    }
    return;
  }

  // Here the prediction is wrong, but user has to provide some value still.
  // This should be a false negative.
  base::UmaHistogramSparse(aggregate_histogram, FALSE_NEGATIVE_MISMATCH);
  // Log FALSE_NEGATIVE_MISMATCH for predicted type if it did predicted
  // something but actual type is different.
  if (predicted_type != UNKNOWN_TYPE) {
    base::UmaHistogramSparse(type_specific_histogram,
                             GetFieldTypeGroupPredictionQualityMetric(
                                 predicted_type, FALSE_NEGATIVE_MISMATCH));
  }
  if (log_rationalization_metrics) {
    // Logging RATIONALIZATION_OK despite of type mismatch here because autofill
    // would have got it wrong with or without rationalization. Rationalization
    // here does not help, neither does it do any harm.
    base::UmaHistogramSparse(kRationalizationQualityHistogram,
                             RATIONALIZATION_OK);
  }
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
      base::UmaHistogramSparse(
          aggregate_histogram,
          (is_empty ? TRUE_NEGATIVE_EMPTY
                    : (is_ambiguous ? TRUE_NEGATIVE_AMBIGUOUS
                                    : TRUE_NEGATIVE_UNKNOWN)));
      return;
    }

    // Log both aggregate and type specific true positive if we correctly
    // predict that type with which the field was filled.
    base::UmaHistogramSparse(aggregate_histogram, TRUE_POSITIVE);
    base::UmaHistogramSparse(
        type_specific_histogram,
        GetFieldTypeGroupPredictionQualityMetric(actual_type, TRUE_POSITIVE));
    return;
  }

  // Note: At this point predicted_type != actual type
  // If actual type is UNKNOWN_TYPE then the prediction is a false positive.
  // Further specialize the type of false positive by whether the field was
  // empty or contained an unknown value.
  if (actual_type == UNKNOWN_TYPE) {
    auto metric = (is_empty ? FALSE_POSITIVE_EMPTY
                            : (is_ambiguous ? FALSE_POSITIVE_AMBIGUOUS
                                            : FALSE_POSITIVE_UNKNOWN));
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
    base::UmaHistogramSparse(aggregate_histogram, FALSE_NEGATIVE_UNKNOWN);
    base::UmaHistogramSparse(type_specific_histogram,
                             GetFieldTypeGroupPredictionQualityMetric(
                                 actual_type, FALSE_NEGATIVE_UNKNOWN));
    return;
  }

  // Note: At this point predicted_type != actual type, actual_type != UNKNOWN,
  //       predicted_type != UNKNOWN.
  // This is a mismatch. From the reference of the actual type, this is a false
  // negative (it was T, but predicted U). From the reference of the prediction,
  // this is a false positive (predicted it was T, but it was U).
  base::UmaHistogramSparse(aggregate_histogram, FALSE_NEGATIVE_MISMATCH);
  base::UmaHistogramSparse(type_specific_histogram,
                           GetFieldTypeGroupPredictionQualityMetric(
                               actual_type, FALSE_NEGATIVE_MISMATCH));
  base::UmaHistogramSparse(type_specific_histogram,
                           GetFieldTypeGroupPredictionQualityMetric(
                               predicted_type, FALSE_POSITIVE_MISMATCH));
}

// Logs field type prediction quality metrics.  The primary histogram name is
// constructed based on |prediction_source| The field-specific histogram names
// also incorporates the possible and predicted types for |field|. A suffix may
// be appended to the metric name, depending on |metric_type|.
void LogPredictionQualityMetrics(
    QualityMetricPredictionSource prediction_source,
    FieldType predicted_type,
    autofill_metrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type,
    bool log_rationalization_metrics) {
  // Generate histogram names.
  const char* source = GetQualityMetricPredictionSource(prediction_source);
  const char* suffix = GetQualityMetricTypeSuffix(metric_type);
  std::string raw_data_histogram =
      base::StrCat({kFieldPredictionMetricPrefix, source, suffix});
  std::string aggregate_histogram =
      base::StrCat({kAggregateFieldPredictionMetricPrefix, source, suffix});
  std::string type_specific_histogram =
      base::StrCat({kByFieldTypeFieldPredictionMetricPrefix, source, suffix});

  const FieldTypeSet& possible_types =
      metric_type == TYPE_AUTOCOMPLETE_BASED
          ? FieldTypeSet{AutofillType(field.html_type()).GetStorableType()}
          : field.possible_types();

  // Get the best type classification we can for the field.
  FieldType actual_type = GetActualFieldType(possible_types, predicted_type);

  DCHECK_LE(predicted_type, UINT16_MAX);
  DCHECK_LE(actual_type, UINT16_MAX);
  base::UmaHistogramSparse(raw_data_histogram,
                           (predicted_type << 16) | actual_type);

  form_interactions_ukm_logger->LogFieldType(
      form.form_parsed_timestamp(), form.form_signature(),
      field.GetFieldSignature(), prediction_source, metric_type, predicted_type,
      actual_type);

  // NO_SERVER_DATA is the equivalent of predicting UNKNOWN.
  if (predicted_type == NO_SERVER_DATA) {
    predicted_type = UNKNOWN_TYPE;
  }

  // The actual type being EMPTY_TYPE is the same as UNKNOWN_TYPE for comparison
  // purposes, but remember whether or not it was empty for more precise logging
  // later.
  bool is_empty = (actual_type == EMPTY_TYPE);
  bool is_ambiguous = (actual_type == AMBIGUOUS_TYPE);
  if (is_empty || is_ambiguous) {
    actual_type = UNKNOWN_TYPE;
  }

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

void LogHeuristicPredictionQualityMetrics(
    autofill_metrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_HEURISTIC, field.heuristic_type(),
      form_interactions_ukm_logger, form, field, metric_type,
      /*log_rationalization_metrics=*/false);
  if (metric_type == TYPE_SUBMISSION) {
    LogHeuristicPredictionQualityPerLabelSourceMetric(field);
  }
}

void LogHeuristicPredictionQualityPerLabelSourceMetric(
    const AutofillField& field) {
  FieldType predicted_type = field.heuristic_type();
  // If there are multiple `possible_types()`, `GetActualFieldType()` will:
  // - Return the `predicted_type` if it is contained in the set. A "true"
  //   sample is emitted to the metric.
  // - Return AMBIGUOUS_TYPE otherwise. A "false" sample is emitted to the
  //   metric.
  FieldType actual_type =
      GetActualFieldType(field.possible_types(), predicted_type);
  if (actual_type != UNKNOWN_TYPE && actual_type != EMPTY_TYPE) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {kAggregateFieldPredictionMetricPrefix,
             GetQualityMetricPredictionSource(PREDICTION_SOURCE_HEURISTIC), ".",
             LabelSourceToString(field.label_source())}),
        predicted_type == actual_type);
  }
}

void LogMlPredictionQualityMetrics(
    autofill_metrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_ML_PREDICTIONS,
      field.heuristic_type(HeuristicSource::kAutofillMachineLearning),
      form_interactions_ukm_logger, form, field, metric_type,
      /*log_rationalization_metrics=*/false);
}

// static
void LogServerPredictionQualityMetrics(
    autofill_metrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(PREDICTION_SOURCE_SERVER, field.server_type(),
                              form_interactions_ukm_logger, form, field,
                              metric_type,
                              /*log_rationalization_metrics=*/false);
}

// static
void LogOverallPredictionQualityMetrics(
    autofill_metrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  LogPredictionQualityMetrics(
      PREDICTION_SOURCE_OVERALL, field.Type().GetStorableType(),
      form_interactions_ukm_logger, form, field, metric_type,
      /*log_rationalization_metrics=*/true);
}

void LogEmailFieldPredictionMetrics(const AutofillField& field) {
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

}  // namespace autofill::autofill_metrics

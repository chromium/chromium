// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/quality_metrics.h"

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"
#include "components/autofill/core/browser/metrics/granular_filling_metrics_utils.h"
#include "components/autofill/core/browser/metrics/precedence_over_autocomplete_metrics.h"
#include "components/autofill/core/browser/metrics/shadow_prediction_metrics.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill::autofill_metrics {

void LogQualityMetrics(
    const FormStructure& form_structure,
    const base::TimeTicks& load_time,
    const base::TimeTicks& interaction_time,
    const base::TimeTicks& submission_time,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    bool did_show_suggestions,
    bool observed_submission,
    const FormInteractionCounts& form_interaction_counts) {
  // Use the same timestamp on UKM Metrics generated within this method's scope.
  AutofillMetrics::UkmTimestampPin timestamp_pin(form_interactions_ukm_logger);

  // Determine the type of the form.
  DenseSet<FormType> form_types = form_structure.GetFormTypes();
  bool card_form = base::Contains(form_types, FormType::kCreditCardForm);
  bool address_form = base::Contains(form_types, FormType::kAddressForm);

  ServerFieldTypeSet autofilled_field_types;
  size_t num_detected_field_types = 0;
  size_t num_edited_autofilled_fields = 0;
  size_t num_of_accepted_autofilled_fields = 0;
  size_t num_of_corrected_autofilled_fields = 0;

  // Tracks how many fields are filled, unfilled or corrected for the address
  // and credit card forms.
  autofill_metrics::FormGroupFillingStats address_field_stats;
  autofill_metrics::FormGroupFillingStats cc_field_stats;

  // Same as above, but keyed by `AutofillFillingMethod`.
  base::flat_map<AutofillFillingMethod, autofill_metrics::FormGroupFillingStats>
      address_field_stats_by_filling_method;

  // Count the number of autofilled and corrected non-credit card fields with
  // ac=unrecognized.
  // Note that this can be misleading, since autocompleted fields count as
  // autofilled.
  size_t num_of_accepted_autofilled_fields_with_autocomplete_unrecognized = 0;
  size_t num_of_corrected_autofilled_fields_with_autocomplete_unrecognized = 0;

  bool did_autofill_all_possible_fields = true;
  bool did_autofill_some_possible_fields = false;
  bool is_for_credit_card = form_structure.IsCompleteCreditCardForm();
  bool has_upi_vpa_field = false;
  bool has_observed_one_time_code_field = false;
  // A perfectly filled form is submitted as it was filled from Autofill without
  // subsequent changes.
  bool perfect_filling = true;
  // Contain the frames across which the fields are distributed.
  base::flat_set<LocalFrameToken> frames_of_detected_fields;
  base::flat_set<LocalFrameToken> frames_of_detected_credit_card_fields;
  base::flat_set<LocalFrameToken> frames_of_autofilled_credit_card_fields;

  // Determine the correct suffix for the metric, depending on whether or
  // not a submission was observed.
  const AutofillMetrics::QualityMetricType metric_type =
      observed_submission ? AutofillMetrics::TYPE_SUBMISSION
                          : AutofillMetrics::TYPE_NO_SUBMISSION;

  for (auto& field : form_structure) {
    DCHECK(field);

    AutofillType type = field->Type();
    const FieldTypeGroup group = type.group();

    if (IsUPIVirtualPaymentAddress(field->value)) {
      has_upi_vpa_field = true;
      AutofillMetrics::LogUserHappinessMetric(
          AutofillMetrics::USER_DID_ENTER_UPI_VPA, group,
          security_state::SecurityLevel::SECURITY_LEVEL_COUNT,
          data_util::DetermineGroups(form_structure));
    }

    form_interactions_ukm_logger->LogFieldFillStatus(form_structure, *field,
                                                     metric_type);

    AutofillMetrics::LogHeuristicPredictionQualityMetrics(
        form_interactions_ukm_logger, form_structure, *field, metric_type);
    AutofillMetrics::LogServerPredictionQualityMetrics(
        form_interactions_ukm_logger, form_structure, *field, metric_type);
    AutofillMetrics::LogOverallPredictionQualityMetrics(
        form_interactions_ukm_logger, form_structure, *field, metric_type);
    autofill_metrics::LogShadowPredictionComparison(*field);
    // We count fields that were autofilled but later modified, regardless of
    // whether the data now in the field is recognized.
    if (field->previously_autofilled()) {
      num_edited_autofilled_fields++;
    }

    if (type.html_type() == HtmlFieldType::kOneTimeCode) {
      has_observed_one_time_code_field = true;
    }

    // The form was not perfectly filled if a non-empty field was not
    // autofilled.
    if (!field->value.empty() && !field->is_autofilled) {
      perfect_filling = false;
    }

    // If the field was identified by heuristic or server predictions as a
    // street name or a house number, log the value of the autocomplete
    // attribute that was used to represent the field.
    if (IsStreetNameOrHouseNumberType(field->server_type()) ||
        IsStreetNameOrHouseNumberType(field->heuristic_type())) {
      autofill_metrics::
          LogHtmlTypesForAutofilledFieldWithStreetNameOrHouseNumberPredictions(
              *field);
    }

    // Field filling statistics that are only emitted if the form was submitted
    // but independent of the existence of a possible type.
    if (observed_submission) {
      // If the field was either autofilled and accepted or corrected, emit the
      // FieldWiseCorrectness metric.
      if (field->is_autofilled || field->previously_autofilled()) {
        AutofillMetrics::LogEditedAutofilledFieldAtSubmission(
            form_interactions_ukm_logger, form_structure, *field);
        // To emit the StreetNameOrHouseNumberPrecedenceCorrectness metric, we
        // should check if the feature
        // `kAutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete` had an
        // effect on the current field. This lambda takes care of that.
        auto precedence_feature_had_effect = [](const AutofillField& field) {
          // When server override happens, `ComputedType()` isn't called and
          // hence the feature's logic doesn't apply.
          bool no_server_override =
              !field.server_type_prediction_is_override() ||
              field.server_type() == NO_SERVER_DATA;
          // When the autocomplete attribute is unspecified, it is
          // unconditionally overridden, regardless of the feature.
          bool specified_autocomplete =
              field.html_type() != HtmlFieldType::kUnspecified;
          // We are not interested in cases where the autocomplete attribute
          // agrees with server or heuristic predictions, since in that case
          // precedence wouldn't change the behavior of the program.
          bool autocomplete_disagree_with_type =
              field.Type().GetStorableType() !=
              AutofillType(field.html_type()).GetStorableType();
          // The feature is only active for street name and house number types.
          bool is_street_name_or_house_number =
              IsStreetNameOrHouseNumberType(field.Type().GetStorableType());

          return no_server_override && specified_autocomplete &&
                 autocomplete_disagree_with_type &&
                 is_street_name_or_house_number;
        };
        if (precedence_feature_had_effect(*field)) {
          autofill_metrics::
              LogEditedAutofilledFieldWithStreetNameOrHouseNumberPrecedenceAtSubmission(
                  *field);
        }
      }

      // For any field that belongs to either an address or a credit card form,
      // collect the type-unspecific field filling statistics.
      // Those are only emitted when autofill was used on at least one field of
      // the form.
      const FormType form_type_of_field = FieldTypeGroupToFormType(group);
      const bool is_address_form_field =
          form_type_of_field == FormType::kAddressForm;
      const bool credit_card_form_field =
          form_type_of_field == FormType::kCreditCardForm;

      if (is_address_form_field || credit_card_form_field) {
        // Address and credit cards fields are mutually exclusive.
        autofill_metrics::FormGroupFillingStats& group_stats =
            is_address_form_field ? address_field_stats : cc_field_stats;

        // Get the filling status of this field and add it to the form group
        // counter.
        group_stats.AddFieldFillingStatus(
            autofill_metrics::GetFieldFillingStatus(*field));
        // For address forms we want to emit filling stats metrics per
        // `AutofillFillingMethod`. Therefore, the stats generated are added to
        // a map keyed by `AutofillFillingMethod`, so that later, metrics can
        // emitted for each method used.
        if (base::FeatureList::IsEnabled(
                features::kAutofillGranularFillingAvailable) &
            is_address_form_field) {
          AddFillingStatsForAutofillFillingMethod(
              *field, address_field_stats_by_filling_method);
        }
      }
    }

    ///////////////////////////////////////////////////////////////////////////
    /// WARNING: Everything below this line is conditioned on having a possible
    /// field type. This means the field must contain a value that can be found
    /// in one of the stored Autofill profiles.
    ///////////////////////////////////////////////////////////////////////////
    const ServerFieldTypeSet& field_types = field->possible_types();
    DCHECK(!field_types.empty());

    // For every field that has a heuristics prediction for a
    // NUMERIC_QUANTITY, log if there was a colliding server
    // prediction and if the NUMERIC_QUANTITY was a false-positive prediction.
    // The latter is true when the field was correctly filled. This can
    // only be recorded when the feature to grant precedence to
    // NUMERIC_QUANTITY predictions is disabled.
    if (observed_submission && field->heuristic_type() == NUMERIC_QUANTITY) {
      bool field_has_non_empty_server_prediction =
          field->server_type() != UNKNOWN_TYPE &&
          field->server_type() != NO_SERVER_DATA;

      // Log if there was a colliding server prediction.
      AutofillMetrics::LogNumericQuantityCollidesWithServerPrediction(
          field_has_non_empty_server_prediction);

      // If there was a collision, log if the NUMERIC_QUANTITY was a false
      // positive since the field was correctly filled.
      if ((field->is_autofilled || field->previously_autofilled()) &&
          field_has_non_empty_server_prediction &&
          !base::FeatureList::IsEnabled(
              features::kAutofillGivePrecedenceToNumericQuantities)) {
        AutofillMetrics::
            LogAcceptedFilledFieldWithNumericQuantityHeuristicPrediction(
                !field->previously_autofilled());
      }
    }

    // Skip all remaining metrics if there wasn't a single possible field type
    // detected.
    if (!FieldHasMeaningfulPossibleFieldTypes(*field)) {
      continue;
    }

    ++num_detected_field_types;

    // Count the number of autofilled and corrected fields.
    // TODO(crbug.com/1368096): This metric is defective because it is falsely
    // conditioned on having a detected field type. The metric is replaced by a
    // new one and the old one should be removed once the new one is fully
    // launched.
    if (field->is_autofilled) {
      ++num_of_accepted_autofilled_fields;
      if (field->ShouldSuppressSuggestionsAndFillingByDefault()) {
        ++num_of_accepted_autofilled_fields_with_autocomplete_unrecognized;
      }
    } else if (field->previously_autofilled()) {
      ++num_of_corrected_autofilled_fields;
      if (field->ShouldSuppressSuggestionsAndFillingByDefault()) {
        ++num_of_corrected_autofilled_fields_with_autocomplete_unrecognized;
      }
    }

    if (field->is_autofilled) {
      did_autofill_some_possible_fields = true;
    } else if (!field->only_fill_when_focused()) {
      did_autofill_all_possible_fields = false;
    }

    if (field->is_autofilled) {
      autofilled_field_types.insert(type.GetStorableType());
    }

    // Keep track of the frames of detected and autofilled (credit card) fields.
    frames_of_detected_fields.insert(field->host_frame);
    if (group == FieldTypeGroup::kCreditCard) {
      frames_of_detected_credit_card_fields.insert(field->host_frame);
      if (field->is_autofilled) {
        frames_of_autofilled_credit_card_fields.insert(field->host_frame);
      }
    }

    if (observed_submission) {
      // If the form was submitted, record if field types have been filled and
      // subsequently edited by the user.
      if (field->is_autofilled || field->previously_autofilled()) {
        // TODO(crbug.com/1368096): This metric is defective because it is
        // conditioned on having a possible field type. Remove after M112.
        AutofillMetrics::LogEditedAutofilledFieldAtSubmissionDeprecated(
            form_interactions_ukm_logger, form_structure, *field);
      }

      base::UmaHistogramEnumeration(
          "Autofill.LabelInference.InferredLabelSource.AtSubmission2",
          field->label_source);
    }
  }

  AutofillMetrics::LogNumberOfEditedAutofilledFields(
      num_edited_autofilled_fields, observed_submission);

  // We log "submission" and duration metrics if we are here after observing a
  // submission event.
  if (observed_submission) {
    AutofillMetrics::AutofillFormSubmittedState state;
    if (num_detected_field_types < kMinRequiredFieldsForHeuristics &&
        num_detected_field_types < kMinRequiredFieldsForQuery) {
      state = AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA;
    } else {
      if (did_autofill_all_possible_fields) {
        state = AutofillMetrics::FILLABLE_FORM_AUTOFILLED_ALL;
      } else if (did_autofill_some_possible_fields) {
        state = AutofillMetrics::FILLABLE_FORM_AUTOFILLED_SOME;
      } else if (!did_show_suggestions) {
        state = AutofillMetrics::
            FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS;
      } else {
        state =
            AutofillMetrics::FILLABLE_FORM_AUTOFILLED_NONE_DID_SHOW_SUGGESTIONS;
      }

      // Log the number of autofilled fields at submission time.
      AutofillMetrics::LogNumberOfAutofilledFieldsAtSubmission(
          num_of_accepted_autofilled_fields,
          num_of_corrected_autofilled_fields);

      // Log the number of autofilled fields with an unrecognized autocomplete
      // attribute at submission time.
      // Note that credit card fields are not counted since they generally
      // ignore an unrecognized autocomplete attribute.
      AutofillMetrics::
          LogNumberOfAutofilledFieldsWithAutocompleteUnrecognizedAtSubmission(
              num_of_accepted_autofilled_fields_with_autocomplete_unrecognized,
              num_of_corrected_autofilled_fields_with_autocomplete_unrecognized);

      // Unlike the other times, the |submission_time| should always be
      // available.
      DCHECK(!submission_time.is_null());

      // The |load_time| might be unset, in the case that the form was
      // dynamically added to the DOM.
      if (!load_time.is_null()) {
        // Submission should always chronologically follow form load.
        DCHECK_GE(submission_time, load_time);
        base::TimeDelta elapsed = submission_time - load_time;
        if (did_autofill_some_possible_fields) {
          AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(elapsed);
        } else {
          AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(elapsed);
        }
      }

      // The |interaction_time| might be unset, in the case that the user
      // submitted a blank form.
      if (!interaction_time.is_null()) {
        // Submission should always chronologically follow interaction.
        DCHECK(submission_time > interaction_time);
        base::TimeDelta elapsed = submission_time - interaction_time;
        AutofillMetrics::LogFormFillDurationFromInteraction(
            form_structure.GetFormTypes(), did_autofill_some_possible_fields,
            elapsed);
      }
    }

    if (has_observed_one_time_code_field) {
      if (!load_time.is_null()) {
        DCHECK_GE(submission_time, load_time);
        base::TimeDelta elapsed = submission_time - load_time;
        AutofillMetrics::LogFormFillDurationFromLoadForOneTimeCode(elapsed);
      }
      if (!interaction_time.is_null()) {
        DCHECK(submission_time > interaction_time);
        base::TimeDelta elapsed = submission_time - interaction_time;
        AutofillMetrics::LogFormFillDurationFromInteractionForOneTimeCode(
            elapsed);
      }
    }

    AutofillMetrics::LogAutofillFormSubmittedState(
        state, is_for_credit_card, has_upi_vpa_field,
        form_structure.GetFormTypes(), form_structure.form_parsed_timestamp(),
        form_structure.form_signature(), form_interactions_ukm_logger,
        form_interaction_counts);

    // The perfect filling metric is only recorded if Autofill was used on at
    // least one field. This conditions this metric on Assistance, Readiness and
    // Acceptance.
    if (did_autofill_some_possible_fields) {
      // Perfect filling is recorded for addresses and credit cards separately.
      // Note that a form can be both an address and a credit card form
      // simultaneously.
      if (address_form) {
        AutofillMetrics::LogAutofillPerfectFilling(/*is_address=*/true,
                                                   perfect_filling);
      }
      if (card_form) {
        AutofillMetrics::LogAutofillPerfectFilling(/*is_address=*/false,
                                                   perfect_filling);
      }
    }

    // Log the field filling statistics if autofill was used.
    // The metrics are only emitted if there was at least one field in the
    // corresponding form group that is or was filled by autofill.
    // TODO(crbug.com/1459990): Remove this metric on cleanup.
    autofill_metrics::LogFieldFillingStatsAndScore(address_field_stats,
                                                   cc_field_stats);
    LogAddressFieldFillingStatsAndScoreByAutofillFillingMethod(
        address_field_stats_by_filling_method);

    if (card_form) {
      AutofillMetrics::LogCreditCardSeamlessnessAtSubmissionTime(
          autofilled_field_types);
    }
  }
}

// Log the quality of the heuristics and server predictions for this form
// structure, if autocomplete attributes are present on the fields (since the
// autocomplete attribute takes precedence over other type predictions).
void LogQualityMetricsBasedOnAutocomplete(
    const FormStructure& form_structure,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  const AutofillMetrics::QualityMetricType metric_type =
      AutofillMetrics::TYPE_AUTOCOMPLETE_BASED;
  for (const auto& field : form_structure) {
    if (field->html_type() != HtmlFieldType::kUnspecified &&
        field->html_type() != HtmlFieldType::kUnrecognized) {
      AutofillMetrics::LogHeuristicPredictionQualityMetrics(
          form_interactions_ukm_logger, form_structure, *field, metric_type);
      AutofillMetrics::LogServerPredictionQualityMetrics(
          form_interactions_ukm_logger, form_structure, *field, metric_type);
    }
  }
}

}  // namespace autofill::autofill_metrics

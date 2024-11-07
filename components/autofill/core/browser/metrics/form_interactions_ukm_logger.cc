// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/prediction_quality_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill::autofill_metrics {

namespace {

// Exponential bucket spacing for UKM event data.
constexpr double kAutofillEventDataBucketSpacing = 2.0;

}  // namespace

FormInteractionsUkmLogger::FormInteractionsUkmLogger(
    AutofillClient* autofill_client,
    ukm::UkmRecorder* ukm_recorder)
    : autofill_client_(autofill_client), ukm_recorder_(ukm_recorder) {}

ukm::builders::Autofill_CreditCardFill
FormInteractionsUkmLogger::CreateCreditCardFillBuilder() {
  return ukm::builders::Autofill_CreditCardFill(GetSourceId());
}

void FormInteractionsUkmLogger::Record(
    ukm::builders::Autofill_CreditCardFill&& builder) {
  if (CanLog()) {
    builder.Record(ukm_recorder_);
  }
}

void FormInteractionsUkmLogger::OnFormsParsed(const ukm::SourceId source_id) {
  if (!CanLog()) {
    return;
  }

  source_id_ = source_id;
}

void FormInteractionsUkmLogger::LogInteractedWithForm(
    bool is_for_credit_card,
    size_t local_record_type_count,
    size_t server_record_type_count,
    FormSignature form_signature) {
  if (!CanLog()) {
    return;
  }

  ukm::builders::Autofill_InteractedWithForm(GetSourceId())
      .SetIsForCreditCard(is_for_credit_card)
      .SetLocalRecordTypeCount(local_record_type_count)
      .SetServerRecordTypeCount(server_record_type_count)
      .SetFormSignature(HashFormSignature(form_signature))
      .Record(ukm_recorder_);
}

void FormInteractionsUkmLogger::LogSuggestionsShown(
    const FormStructure& form,
    const AutofillField& field,
    base::TimeTicks form_parsed_timestamp,
    bool off_the_record) {
  if (!CanLog()) {
    return;
  }

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

void FormInteractionsUkmLogger::LogDidFillSuggestion(
    const FormStructure& form,
    const AutofillField& field,
    std::optional<CreditCard::RecordType> record_type) {
  if (!CanLog()) {
    return;
  }

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

void FormInteractionsUkmLogger::LogEditedAutofilledFieldAtSubmission(
    const FormStructure& form,
    const AutofillField& field) {
  if (!CanLog()) {
    return;
  }

  ukm::builders::Autofill_EditedAutofilledFieldAtSubmission(GetSourceId())
      .SetFieldSignature(HashFieldSignature(field.GetFieldSignature()))
      .SetFormSignature(HashFormSignature(form.form_signature()))
      .SetOverallType(static_cast<int64_t>(field.Type().GetStorableType()))
      .Record(ukm_recorder_);
}

void FormInteractionsUkmLogger::LogTextFieldDidChange(
    const FormStructure& form,
    const AutofillField& field) {
  if (!CanLog()) {
    return;
  }

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

void FormInteractionsUkmLogger::LogFieldFillStatus(
    const FormStructure& form,
    const AutofillField& field,
    QualityMetricType metric_type) {
  if (!CanLog()) {
    return;
  }

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
void FormInteractionsUkmLogger::LogFieldType(
    base::TimeTicks form_parsed_timestamp,
    FormSignature form_signature,
    FieldSignature field_signature,
    QualityMetricPredictionSource prediction_source,
    QualityMetricType metric_type,
    FieldType predicted_type,
    FieldType actual_type) {
  if (!CanLog()) {
    return;
  }

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

void FormInteractionsUkmLogger::LogAutofillFieldInfoAtFormRemove(
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

  DenseSet<AutofillStatus> autofill_status_vector;
  auto SetStatusVector = [&autofill_status_vector](AutofillStatus status,
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
        case HeuristicSource::kAutofillMachineLearning:
        case HeuristicSource::kPasswordManagerMachineLearning:
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
    SetStatusVector(AutofillStatus::kWasAutofillTriggeredOnField,
                    OptionalBooleanToBool(suggestion_was_accepted));
  }

  SetStatusVector(AutofillStatus::kWasAutofillTriggeredAnywhereOnForm,
                  autofill_count > 0);
  if (autofill_count > 0) {
    SetStatusVector(
        AutofillStatus::kShouldBeAutofilledBeforeSecurityPolicy,
        OptionalBooleanToBool(was_autofilled_before_security_policy));
    SetStatusVector(AutofillStatus::kHadValueBeforeFilling,
                    OptionalBooleanToBool(had_value_before_filling));
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

void FormInteractionsUkmLogger::LogAutofillFormSummaryAtFormRemove(
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

void FormInteractionsUkmLogger::
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
    return !value.empty() && MatchesRegex(value, pattern);
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

void FormInteractionsUkmLogger::LogFocusedComplexFormAtFormRemove(
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

void FormInteractionsUkmLogger::LogHiddenRepresentationalFieldSkipDecision(
    const FormStructure& form,
    const AutofillField& field,
    bool is_skipped) {
  if (!CanLog()) {
    return;
  }

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

ukm::SourceId FormInteractionsUkmLogger::GetSourceId() {
  if (!source_id_.has_value()) {
    source_id_ = autofill_client_->GetUkmSourceId();
  }
  return *source_id_;
}

void FormInteractionsUkmLogger::LogKeyMetrics(
    const DenseSet<FormTypeNameForLogging>& form_types,
    bool data_to_fill_available,
    bool suggestions_shown,
    bool edited_autofilled_field,
    bool suggestion_filled,
    const FormInteractionCounts& form_interaction_counts,
    const FormInteractionsFlowId& flow_id,
    std::optional<int64_t> fast_checkout_run_id) {
  if (!CanLog()) {
    return;
  }

  ukm::builders::Autofill_KeyMetrics builder(GetSourceId());
  builder.SetFillingReadiness(data_to_fill_available)
      .SetFillingAssistance(suggestion_filled)
      .SetFormTypes(AutofillMetrics::FormTypesToBitVector(form_types))
      .SetAutofillFills(form_interaction_counts.autofill_fills)
      .SetFormElementUserModifications(
          form_interaction_counts.form_element_user_modifications)
      .SetFlowId(flow_id.value());
  if (fast_checkout_run_id) {
    builder.SetFastCheckoutRunId(fast_checkout_run_id.value());
  }
  if (suggestions_shown) {
    builder.SetFillingAcceptance(suggestion_filled);
  }

  if (suggestion_filled) {
    builder.SetFillingCorrectness(!edited_autofilled_field);
  }

  builder.Record(ukm_recorder_);
}

void FormInteractionsUkmLogger::LogFormEvent(
    autofill_metrics::FormEvent form_event,
    const DenseSet<FormTypeNameForLogging>& form_types,
    base::TimeTicks form_parsed_timestamp) {
  if (!CanLog()) {
    return;
  }

  if (form_parsed_timestamp.is_null()) {
    return;
  }

  ukm::builders::Autofill_FormEvent builder(GetSourceId());
  builder.SetAutofillFormEvent(static_cast<int>(form_event))
      .SetFormTypes(AutofillMetrics::FormTypesToBitVector(form_types))
      .SetMillisecondsSinceFormParsed(
          MillisecondsSinceFormParsed(form_parsed_timestamp))
      .Record(ukm_recorder_);
}

bool FormInteractionsUkmLogger::CanLog() const {
  return ukm_recorder_ != nullptr;
}

int64_t FormInteractionsUkmLogger::MillisecondsSinceFormParsed(
    base::TimeTicks form_parsed_timestamp) const {
  DCHECK(!form_parsed_timestamp.is_null());
  // Use the pinned timestamp as the current time if it's set.
  base::TimeTicks now =
      pinned_timestamp_.is_null() ? base::TimeTicks::Now() : pinned_timestamp_;

  return ukm::GetExponentialBucketMin(
      (now - form_parsed_timestamp).InMilliseconds(),
      kAutofillEventDataBucketSpacing);
}

UkmTimestampPin::UkmTimestampPin(
    autofill_metrics::FormInteractionsUkmLogger* logger)
    : logger_(logger) {
  DCHECK(logger_);
  DCHECK(!logger_->has_pinned_timestamp());
  logger_->set_pinned_timestamp(base::TimeTicks::Now());
}

UkmTimestampPin::~UkmTimestampPin() {
  DCHECK(logger_->has_pinned_timestamp());
  logger_->set_pinned_timestamp(base::TimeTicks());
}

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

}  // namespace autofill::autofill_metrics

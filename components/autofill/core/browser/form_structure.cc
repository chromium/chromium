// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_ai_form_rationalization.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_server_prediction.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/crowdsourcing/server_prediction_overrides.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_processing/label_processing_util.h"
#include "components/autofill/core/browser/form_processing/name_processing_util.h"
#include "components/autofill/core/browser/form_structure_rationalizer.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/prediction_quality_metrics.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/security_state/core/security_state.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/origin.h"

namespace autofill {

using mojom::SubmissionIndicatorEvent;

namespace {

std::string ServerTypesToString(const AutofillField& field) {
  std::vector<std::string_view> server_types =
      base::ToVector(field.server_predictions(), [](const auto& prediction) {
        return FieldTypeToStringView(
            ToSafeFieldType(prediction.type(), NO_SERVER_DATA));
      });

  if (server_types.empty()) {
    return "pending";
  }

  return base::StrCat({"[", base::JoinString(server_types, ", "), "]"});
}

std::string AttributeTypesToString(base::span<const AttributeType> types) {
  auto attribute_type_to_string = [](AttributeType t) {
    return base::StrCat(
        {t.entity_type().name_as_string(), ": ", t.name_as_string()});
  };
  return base::StrCat(
      {"[",
       base::JoinString(base::ToVector(types, attribute_type_to_string), ", "),
       "]"});
}

std::string_view ToYesOrNo(bool value) {
  return value ? "Yes" : "No";
}

}  // namespace

FormStructure::FormStructure(const FormData& form)
    : id_attribute_(form.id_attribute()),
      name_attribute_(form.name_attribute()),
      form_name_(form.name()),
      button_titles_(form.button_titles()),
      source_url_(form.url()),
      full_source_url_(form.full_url()),
      target_url_(form.action()),
      main_frame_origin_(form.main_frame_origin()),
      form_parsed_timestamp_(base::TimeTicks::Now()),
      host_frame_(form.host_frame()),
      version_(form.version()),
      renderer_id_(form.renderer_id()),
      child_frames_(form.child_frames()) {
  // Copy the form fields.
  for (const FormFieldData& field : form.fields()) {
    fields_.push_back(std::make_unique<AutofillField>(field));
  }

  form_signature_ = CalculateFormSignature(form);
  alternative_form_signature_ = CalculateAlternativeFormSignature(form);
  structural_form_signature_ = CalculateStructuralFormSignature(form);
  // Do further processing on the fields, as needed.
  SetFieldTypesFromAutocompleteAttribute();
  DetermineFieldRanks();
}

FormStructure::FormStructure(
    FormSignature form_signature,
    const std::vector<FieldSignature>& field_signatures)
    : form_signature_(form_signature) {
  for (const auto& signature : field_signatures) {
    fields_.push_back(AutofillField::CreateForPasswordManagerUpload(signature));
  }
  DetermineFieldRanks();
}

FormStructure::~FormStructure() = default;

void FormStructure::DetermineFieldRanks() {
  size_t rank = 0;
  std::map<FormGlobalId, size_t> rank_in_host_form;
  std::map<FieldSignature, size_t> rank_in_signature_group;
  std::map<std::pair<FormGlobalId, FieldSignature>, size_t>
      rank_in_host_form_signature_group;

  for (auto& field : fields_) {
    field->set_rank(rank++);
    field->set_rank_in_host_form(
        rank_in_host_form[field->renderer_form_id()]++);
    field->set_rank_in_signature_group(
        rank_in_signature_group[field->GetFieldSignature()]++);
    field->set_rank_in_host_form_signature_group(
        rank_in_host_form_signature_group[std::make_pair(
            field->renderer_form_id(), field->GetFieldSignature())]++);
  }
}

void FormStructure::RationalizeAndAssignSections(
    const GeoIpCountryCode& client_country,
    const LanguageCode& current_page_language,
    LogManager* log_manager) {
  // We call AssignSections() before *and* after rationalization because
  // - rationalization depends on sections and
  // - sectioning depends on field types, which rationalization may change.
  AssignSections(fields_);
  FormStructureRationalizer rationalizer(fields_);
  // Rationalize the form's autocomplete attributes, repeated fields and field
  // type predictions.
  rationalizer.RationalizeContentEditables(log_manager);
  rationalizer.RationalizeAutocompleteAttributes(log_manager);
  rationalizer.RationalizeFieldTypePredictions(
      main_frame_origin(), client_country, current_page_language, log_manager);
  // Rationalize phone number fields so that, in every section, only the first
  // complete phone number is filled automatically. This is useful for when a
  // form contains a first phone number and second phone number, which usually
  // should be distinct.
  rationalizer.RationalizePhoneNumbersForFilling();
  AssignSections(fields_);

  // Log the field type predicted by rationalization.
  // The sections are mapped to consecutive natural numbers starting at 1.
  std::map<Section, size_t> section_id_map;
  for (const auto& field : fields_) {
    if (!base::Contains(section_id_map, field->section())) {
      size_t next_section_id = section_id_map.size() + 1;
      section_id_map[field->section()] = next_section_id;
    }
    for (FieldType field_type : field->Type().GetTypes()) {
      field->AppendLogEventIfNotRepeated(RationalizationFieldLogEvent{
          .field_type = field_type,
          .section_id = section_id_map[field->section()],
          .type_changed = field->Type().GetTypes().contains(field_type) !=
                          field->ComputedType().GetTypes().contains(field_type),
      });
    }
  }
}

FormDataPredictions FormStructure::GetFieldTypePredictions() const {
  CHECK(base::FeatureList::IsEnabled(
      features::debug::kAutofillShowTypePredictions));
  FormDataPredictions form;
  form.data = ToFormData();
  form.signature = FormSignatureAsStr();
  form.alternative_signature =
      base::NumberToString(alternative_form_signature().value());
  form.structural_form_signature =
      base::NumberToString(structural_form_signature().value());

  std::map<const AutofillField*, std::vector<AttributeType>>
      field_to_attribute_types;
  for (const auto& [section, entities_and_fields] :
       RationalizeAndDetermineAttributeTypes(fields())) {
    for (const auto& [entity, fields] : entities_and_fields) {
      for (const AutofillFieldWithAttributeType& f : fields) {
        field_to_attribute_types[&*f.field].push_back(f.type);
      }
    }
  }

  const base::flat_map<FieldGlobalId, std::u16string> parseable_names =
      GetParseableNames(fields_);

  const base::flat_map<FieldGlobalId, std::u16string> parseable_labels =
      GetParseableLabels(fields_);

  for (const auto& field : fields_) {
    FormFieldDataPredictions annotated_field;
    annotated_field.host_form_signature =
        base::NumberToString(field->host_form_signature().value());
    annotated_field.signature = field->FieldSignatureAsStr();
    annotated_field.heuristic_type =
        FieldTypeToStringView(field->heuristic_type());
    annotated_field.pwm_ml_type = FieldTypeToStringView(field->heuristic_type(
        HeuristicSource::kPasswordManagerMachineLearning));
    if (!field->server_predictions().empty()) {
      annotated_field.server_type = FieldTypeToStringView(field->server_type());
    }
    if (auto it = field_to_attribute_types.find(&*field);
        it != field_to_attribute_types.end()) {
      annotated_field.attribute_types = AttributeTypesToString(it->second);
    }
    if (base::optional_ref<const AutofillFormatString> format_string =
            field->format_string()) {
      annotated_field.format_string = base::UTF16ToUTF8(format_string->value);
    }
    annotated_field.html_type = FieldTypeToStringView(field->html_type());
    annotated_field.overall_type = [&] {
      AutofillType overall_type = field->Type();
      if (FieldTypeSet field_types = overall_type.GetTypes();
          field_types.size() > 1 &&
          base::FeatureList::IsEnabled(
              features::debug::
                  kAutofillUnionTypesSingleTypeInAutofillInformation)) {
        return FieldTypeToString(*field_types.begin());
      }
      return overall_type.ToString();
    }();

    annotated_field.parseable_name = base::UTF16ToUTF8([&]() {
      if (auto it = parseable_names.find(field->global_id());
          it != parseable_names.end()) {
        return it->second;
      }
      return field->name();
    }());
    annotated_field.parseable_label = base::UTF16ToUTF8([&]() {
      if (auto it = parseable_labels.find(field->global_id());
          it != parseable_labels.end()) {
        return it->second;
      }
      return field->label();
    }());
    annotated_field.section = field->section().ToString();
    annotated_field.rank = field->rank();
    annotated_field.rank_in_signature_group = field->rank_in_signature_group();
    annotated_field.rank_in_host_form = field->rank_in_host_form();
    annotated_field.rank_in_host_form_signature_group =
        field->rank_in_host_form_signature_group();
    form.fields.push_back(annotated_field);
  }
  return form;
}

// static
std::vector<FieldGlobalId> FormStructure::FindFieldsEligibleForManualFilling(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms) {
  std::vector<FieldGlobalId> fields_eligible_for_manual_filling;
  for (const FormStructure* form : forms) {
    for (const auto& field : form->fields_) {
      FieldTypeGroup field_type_group =
          GroupTypeOfFieldType(field->server_type());
      // In order to trigger the payments bottom sheet that assists users to
      // manually fill the form, credit card form fields are marked eligible for
      // manual filling. Also, if a field is not classified to a type, we can
      // assume that the prediction failed and thus mark it eligible for manual
      // filling. As more form types support manual filling on form interaction,
      // this list may expand in the future.
      if (field_type_group == FieldTypeGroup::kCreditCard ||
          field_type_group == FieldTypeGroup::kNoGroup) {
        fields_eligible_for_manual_filling.push_back(field->global_id());
      }
    }
  }
  return fields_eligible_for_manual_filling;
}

std::unique_ptr<FormStructure> FormStructure::CreateForPasswordManagerUpload(
    FormSignature form_signature,
    const std::vector<FieldSignature>& field_signatures) {
  return base::WrapUnique(new FormStructure(form_signature, field_signatures));
}

std::string FormStructure::FormSignatureAsStr() const {
  return base::NumberToString(form_signature().value());
}

bool FormStructure::IsCompleteCreditCardForm(
    CreditCardFormCompleteness credit_card_form_completeness) const {
  FieldTypeSet all_cc_types = FieldTypeSet(fields_, [](const auto& field) {
    return field->Type().GetCreditCardType();
  });
  all_cc_types.erase(UNKNOWN_TYPE);

  const bool found_cc_expiration =
      std::ranges::any_of(all_cc_types, &data_util::IsCreditCardExpirationType);
  const bool found_cc_number = all_cc_types.contains(CREDIT_CARD_NUMBER);
  switch (credit_card_form_completeness) {
    case CreditCardFormCompleteness::kCompleteCreditCardForm:
      return found_cc_expiration && found_cc_number;
    case CreditCardFormCompleteness::
        kCompleteCreditCardFormIncludingCvcAndName: {
      const bool found_cc_cvc =
          all_cc_types.contains(CREDIT_CARD_VERIFICATION_CODE);
      const bool found_cc_name =
          all_cc_types.contains(CREDIT_CARD_NAME_FULL) ||
          (all_cc_types.contains(CREDIT_CARD_NAME_FIRST) &&
           all_cc_types.contains(CREDIT_CARD_NAME_LAST));
      return found_cc_expiration && found_cc_number && found_cc_cvc &&
             found_cc_name;
    }
  }
}

void FormStructure::RetrieveFromCache(const FormStructure& cached_form,
                                      RetrieveFromCacheReason reason) {
  // Build a table to lookup AutofillFields by their FieldGlobalId.
  auto cached_fields_by_id =
      base::MakeFlatMap<FieldGlobalId, const AutofillField*>(
          cached_form.fields(), {},
          [](const std::unique_ptr<AutofillField>& field) {
            return std::make_pair(field->global_id(), field.get());
          });

  // Lookup field by global_id in cached_fields_by_id.
  auto find_field_by_id = [&cached_fields_by_id](FieldGlobalId global_id) {
    const auto& it = cached_fields_by_id.find(global_id);
    return it != cached_fields_by_id.end() ? it->second : nullptr;
  };

  // Lookup field by field signature and return it in case only a single field
  // with the signature exists.
  auto find_field_with_unique_field_signature =
      [&cached_fields_by_id](
          FieldSignature field_signature) -> const AutofillField* {
    const AutofillField* match = nullptr;
    // Iterate over the fields to find the field with the same form signature.
    for (const auto& entry : cached_fields_by_id) {
      if (entry.second->GetFieldSignature() == field_signature) {
        // If there are multiple matches, do not retrieve the field and stop
        // the process.
        if (match) {
          return nullptr;
        }
        match = entry.second;
      }
    }
    return match;
  };

  for (auto& field : *this) {
    const AutofillField* cached_field = find_field_by_id(field->global_id());

    // If the unique renderer id (or the name) is not stable due to some Java
    // Script magic in the website, use the field signature as a fallback
    // solution to find the field in the cached form.
    if (!cached_field) {
      cached_field =
          find_field_with_unique_field_signature(field->GetFieldSignature());
    }

    // Skip fields that we could not find.
    if (!cached_field) {
      continue;
    }

    field->set_initial_value(cached_field->initial_value(),
                             /*pass_key=*/{});
    field->set_server_predictions(cached_field->server_predictions());
    if (reason == RetrieveFromCacheReason::kFormCacheUpdateWithoutParsing ||
        reason == RetrieveFromCacheReason::kFormCacheUpdateAfterParsing) {
      field->set_is_autofilled(cached_field->is_autofilled());
    }
    field->set_autofill_source_profile_guid(
        cached_field->autofill_source_profile_guid());
    field->set_autofilled_type(cached_field->autofilled_type());
    field->set_filling_product(cached_field->filling_product());
    field->set_previously_autofilled(cached_field->previously_autofilled());
    field->set_did_trigger_suggestions(cached_field->did_trigger_suggestions());
    field->set_was_focused(cached_field->was_focused());
    if (base::optional_ref<const AutofillFormatString> format_string =
            cached_field->format_string()) {
      field->set_format_string_unless_overruled(
          *format_string, cached_field->format_string_source());
    }

    // During form parsing, we don't care for heuristic field classifications
    // and information derived from the autocomplete attribute as those are
    // either regenerated or copied from the form that the renderer sent.
    // During import, no parsing happens and we want to preserve the last field
    // classification. Similarly, if the renderer sends an update that does not
    // trigger parsing, we want to preserve the last field classification
    if (reason == RetrieveFromCacheReason::kFormCacheUpdateWithoutParsing ||
        reason == RetrieveFromCacheReason::kFormImport) {
      // Transfer attributes of the cached AutofillField to the newly created
      // AutofillField.
      for (int i = 0; i <= static_cast<int>(HeuristicSource::kMaxValue); ++i) {
        HeuristicSource s = static_cast<HeuristicSource>(i);
        field->set_heuristic_type(s, cached_field->heuristic_type(s));
      }
      std::optional<FieldTypeSet> cached_ml_types =
          cached_field->ml_supported_types();
      if (cached_ml_types.has_value()) {
        field->set_ml_supported_types(cached_ml_types.value());
      }
      field->SetHtmlType(cached_field->html_type(), cached_field->html_mode());
      field->set_credit_card_number_offset(
          cached_field->credit_card_number_offset());
      field->set_section(cached_field->section());
      field->set_only_fill_when_focused(cached_field->only_fill_when_focused());

      // During import, the final field type is used to decide which
      // information to store in an address profile or credit card. As
      // rationalization is an important component of determining the final
      // field type, the output should be preserved.
      field->SetTypeTo(cached_field->Type(), cached_field->PredictionSource());
    }
    field->set_field_log_events(cached_field->field_log_events());
  }

  // Preserve timestamp from the cache as a new form from the renderer does not
  // know the parsing/filling history, as this information is computed in the
  // browser.
  form_parsed_timestamp_ =
      std::min(form_parsed_timestamp_, cached_form.form_parsed_timestamp_);
  last_filling_timestamp_ = cached_form.last_filling_timestamp_;

  // The form signature should match between query and upload requests to the
  // server. On many websites, form elements are dynamically added, removed, or
  // rearranged via JavaScript between page load and form submission, so we
  // copy over the |form_signature_field_names_| corresponding to the query
  // request.
  form_signature_ = cached_form.form_signature_;

  // Keeping the behavior for structural signature consistent with the main one.
  // In practice, first-encountered signatures are preserved only for purely
  // credit card forms.
  // TODO(crbug.com/431754194): Investigate making the behavior consistent
  // across all form types.
  structural_form_signature_ = cached_form.structural_form_signature_;

  // Whether the AutofillAI model may be run is set at the same time as the
  // server predictions - it also needs to be retrieved from the cache.
  may_run_autofill_ai_model_ = cached_form.may_run_autofill_ai_model_;
}

void FormStructure::SetFieldTypesFromAutocompleteAttribute() {
  std::map<FieldSignature, size_t> field_rank_map;
  for (const std::unique_ptr<AutofillField>& field : fields_) {
    if (!field->parsed_autocomplete()) {
      continue;
    }

    // A parsable autocomplete value was specified. Even an invalid field_type
    // is considered a type hint. This allows a website's author to specify an
    // attribute like autocomplete="other" on a field to disable all Autofill
    // heuristics for the form.
    if (field->parsed_autocomplete()->field_type ==
        HtmlFieldType::kUnspecified) {
      continue;
    }

    field->SetHtmlType(field->parsed_autocomplete()->field_type,
                       field->parsed_autocomplete()->mode);

    // Log the field type predicted from autocomplete attribute.
    ++field_rank_map[field->GetFieldSignature()];
    field->AppendLogEventIfNotRepeated(AutocompleteAttributeFieldLogEvent{
        .html_type = field->parsed_autocomplete()->field_type,
        .html_mode = field->parsed_autocomplete()->mode,
        .rank_in_field_signature_group =
            field_rank_map[field->GetFieldSignature()],
    });
  }
}

const AutofillField* FormStructure::field(size_t index) const {
  if (index >= fields_.size()) {
    NOTREACHED();
  }
  return fields_[index].get();
}

AutofillField* FormStructure::field(size_t index) {
  return const_cast<AutofillField*>(std::as_const(*this).field(index));
}

size_t FormStructure::field_count() const {
  return fields_.size();
}

const AutofillField* FormStructure::GetFieldById(FieldGlobalId field_id) const {
  auto it = std::ranges::find(fields_, field_id, &FormFieldData::global_id);
  return it != fields_.end() ? it->get() : nullptr;
}

AutofillField* FormStructure::GetFieldById(FieldGlobalId field_id) {
  return const_cast<AutofillField*>(
      std::as_const(*this).GetFieldById(field_id));
}

bool FormStructure::is_form_element() const {
  return !renderer_id_.is_null() ||
         (!fields_.empty() &&
          fields_.begin()->get()->form_control_type() ==
              FormControlType::kContentEditable &&
          *fields_.begin()->get()->renderer_id() == *renderer_id_);
}

FormData FormStructure::ToFormData() const {
  FormData data;
  data.set_id_attribute(id_attribute_);
  data.set_name_attribute(name_attribute_);
  data.set_name(form_name_);
  data.set_button_titles(button_titles_);
  data.set_url(source_url_);
  data.set_full_url(full_source_url_);
  data.set_action(target_url_);
  data.set_main_frame_origin(main_frame_origin_);
  data.set_renderer_id(renderer_id_);
  data.set_host_frame(host_frame_);
  data.set_version(version_);
  data.set_child_frames(child_frames_);
  std::vector<FormFieldData> fields;
  fields.reserve(fields_.size());
  for (const auto& field : fields_) {
    fields.push_back(*field);
  }
  data.set_fields(std::move(fields));
  return data;
}

DenseSet<FormType> FormStructure::GetFormTypes() const {
  DenseSet<FormType> form_types;
  for (const auto& field : fields_) {
    if (field->ShouldSuppressSuggestionsAndFillingByDefault()) {
      // Types are predicted for fields with unrecognized autocomplete
      // attribute, but suggestions are suppressed. So we don't want such fields
      // to affect the key and quality metrics. We therefore exclude them from
      // the form types.
      form_types.insert(FormType::kUnknownFormType);
    } else {
      form_types.insert_all([&field] {
        DenseSet<FormType> ts = field->Type().GetFormTypes();
        if (ts.empty()) {
          ts = {FormType::kUnknownFormType};
        }
        return ts;
      }());
    }
  }
  return form_types;
}

std::ostream& operator<<(std::ostream& buffer, const FormStructure& form) {
  buffer << "\nForm signature: "
         << base::StrCat({base::NumberToString(form.form_signature().value()),
                          " - ",
                          base::NumberToString(
                              HashFormSignature(form.form_signature()))});
  buffer << "\n Form name: " << form.form_name();
  buffer << "\n Identifiers: "
         << base::StrCat(
                {"renderer id: ",
                 base::NumberToString(form.global_id().renderer_id.value()),
                 ", host frame: ", form.global_id().frame_token.ToString(),
                 " (", url::Origin::Create(form.source_url()).Serialize(),
                 ")"});
  buffer << "\n Target URL:" << form.target_url();
  if (base::FeatureList::IsEnabled(features::kAutofillAiServerModel)) {
    buffer << "\n May run AutofillAI model: "
           << ToYesOrNo(form.may_run_autofill_ai_model());
  }
  for (size_t i = 0; i < form.field_count(); ++i) {
    buffer << "\n Field " << i << ": ";
    const AutofillField* field = form.field(i);
    buffer << "\n  Identifiers:"
           << base::StrCat(
                  {"renderer id: ",
                   base::NumberToString(field->renderer_id().value()),
                   ", host frame: ",
                   field->renderer_form_id().frame_token.ToString(), " (",
                   field->origin().Serialize(), "), host form renderer id: ",
                   base::NumberToString(field->host_form_id().value())});
    buffer << "\n  Signature: "
           << base::StrCat(
                  {base::NumberToString(field->GetFieldSignature().value()),
                   " - ",
                   base::NumberToString(
                       HashFieldSignature(field->GetFieldSignature())),
                   ", host form signature: ",
                   base::NumberToString(field->host_form_signature().value()),
                   " - ",
                   base::NumberToString(
                       HashFormSignature(field->host_form_signature()))});
    buffer << "\n  Name: " << field->name();

    auto regex_heuristic_type =
        FieldTypeToStringView(field->heuristic_type(HeuristicSource::kRegexes));
    std::string ml_heuristic_part;
    if (features::kAutofillModelPredictionsAreActive.Get()) {
      auto ml_heuristic_type = FieldTypeToStringView(
          field->heuristic_type(HeuristicSource::kAutofillMachineLearning));
      ml_heuristic_part = base::StrCat({", ML heuristic: ", ml_heuristic_type});
      if (ml_heuristic_type != regex_heuristic_type) {
        ml_heuristic_part =
            base::StrCat({ml_heuristic_part, ", overall heuristic: ",
                          FieldTypeToStringView(field->heuristic_type())});
      }
    }
    std::string server_type = ServerTypesToString(*field);
    const char* is_override =
        field->server_type_prediction_is_override() ? " (manual override)" : "";
    auto html_type_description =
        field->html_type() != HtmlFieldType::kUnspecified
            ? base::StrCat(
                  {", html: ", FieldTypeToStringView(field->html_type())})
            : "";
    if (field->html_type() == HtmlFieldType::kUnrecognized &&
        !field->server_type_prediction_is_override()) {
      html_type_description += " (disabling autofill)";
    }

    buffer << "\n  Type: "
           << base::StrCat({field->Type().ToString(),
                            " (regex heuristic: ", regex_heuristic_type,
                            ml_heuristic_part, ", server: ", server_type,
                            is_override, html_type_description, ")"});
    buffer << "\n  Section: " << field->section();

    constexpr size_t kMaxLabelSize = 100;
    const std::u16string truncated_label = field->label().substr(
        0, std::min(field->label().length(), kMaxLabelSize));
    buffer << "\n  Label: " << truncated_label;

    buffer << "\n  Is empty: " << ToYesOrNo(field->value().empty());
  }
  return buffer;
}

LogBuffer& operator<<(LogBuffer& buffer, const FormStructure& form) {
  buffer << Tag{"div"} << Attrib{"class", "form"};
  buffer << Tag{"table"};
  buffer << Tr{} << "Form signature:"
         << base::StrCat({base::NumberToString(form.form_signature().value()),
                          " - ",
                          base::NumberToString(
                              HashFormSignature(form.form_signature()))});
  buffer << Tr{} << "Form alternative signature:"
         << base::StrCat({base::NumberToString(
                              form.alternative_form_signature().value()),
                          " - ",
                          base::NumberToString(HashFormSignature(
                              form.alternative_form_signature()))});
  buffer << Tr{} << "Form name:" << form.form_name();
  buffer << Tr{} << "Identifiers: "
         << base::StrCat(
                {"renderer id: ",
                 base::NumberToString(form.global_id().renderer_id.value()),
                 ", host frame: ", form.global_id().frame_token.ToString(),
                 " (", url::Origin::Create(form.source_url()).Serialize(),
                 ")"});
  buffer << Tr{} << "Target URL:" << form.target_url();
  if (base::FeatureList::IsEnabled(features::kAutofillAiServerModel)) {
    buffer << Tr{} << "May run AutofillAI model: "
           << ToYesOrNo(form.may_run_autofill_ai_model());
  }

  std::map<const AutofillField*, std::vector<AttributeType>>
      field_to_attribute_types;
  for (const auto& [section, entities_and_fields] :
       RationalizeAndDetermineAttributeTypes(form.fields())) {
    for (const auto& [entity, fields] : entities_and_fields) {
      for (const AutofillFieldWithAttributeType& f : fields) {
        field_to_attribute_types[&*f.field].push_back(f.type);
      }
    }
  }

  const base::flat_map<FieldGlobalId, std::u16string> parseable_names =
      GetParseableNames(form.fields());

  const base::flat_map<FieldGlobalId, std::u16string> parseable_labels =
      GetParseableLabels(form.fields());

  for (size_t i = 0; i < form.field_count(); ++i) {
    const AutofillField* field = form.field(i);
    const std::u16string& name = [&]() -> const std::u16string& {
      if (auto it = parseable_names.find(field->global_id());
          it != parseable_names.end()) {
        return it->second;
      }
      return field->name();
    }();
    buffer << Tag{"tr"};
    buffer << Tag{"td"} << "Field " << i << ": " << CTag{};
    buffer << Tag{"td"};
    buffer << Tag{"table"};
    buffer << Tr{} << "Identifiers:"
           << base::StrCat(
                  {"renderer id: ",
                   base::NumberToString(field->renderer_id().value()),
                   ", host frame: ",
                   field->renderer_form_id().frame_token.ToString(), " (",
                   field->origin().Serialize(), "), host form renderer id: ",
                   base::NumberToString(field->host_form_id().value())});
    buffer << Tr{} << "Signature:"
           << base::StrCat(
                  {base::NumberToString(field->GetFieldSignature().value()),
                   " - ",
                   base::NumberToString(
                       HashFieldSignature(field->GetFieldSignature())),
                   ", host form signature: ",
                   base::NumberToString(field->host_form_signature().value()),
                   " - ",
                   base::NumberToString(
                       HashFormSignature(field->host_form_signature()))});
    buffer << Tr{} << "Name:" << name;
    buffer << Tr{} << "Placeholder:" << field->placeholder();

    auto regex_heuristic_type =
        FieldTypeToStringView(field->heuristic_type(HeuristicSource::kRegexes));
    std::string ml_heuristic_part;
    if (features::kAutofillModelPredictionsAreActive.Get()) {
      auto ml_heuristic_type = FieldTypeToStringView(
          field->heuristic_type(HeuristicSource::kAutofillMachineLearning));
      ml_heuristic_part = base::StrCat({", ML heuristic: ", ml_heuristic_type});
      if (ml_heuristic_type != regex_heuristic_type) {
        ml_heuristic_part =
            base::StrCat({ml_heuristic_part, ", overall heuristic: ",
                          FieldTypeToStringView(field->heuristic_type())});
      }
    }
    std::string server_type = ServerTypesToString(*field);
    if (field->server_type_prediction_is_override()) {
      server_type += " (manual override)";
    }
    auto html_type_description =
        field->html_type() != HtmlFieldType::kUnspecified
            ? base::StrCat(
                  {", html: ", FieldTypeToStringView(field->html_type())})
            : "";
    if (field->html_type() == HtmlFieldType::kUnrecognized &&
        !field->server_type_prediction_is_override()) {
      html_type_description += " (disabling autofill)";
    }

    buffer << Tr{} << "Type:"
           << base::StrCat({field->Type().ToString(),
                            " (regex heuristic: ", regex_heuristic_type,
                            ml_heuristic_part, ", server: ", server_type,
                            html_type_description, ")"});
    if (auto it = field_to_attribute_types.find(&*field);
        it != field_to_attribute_types.end()) {
      buffer << Tr{} << "Autofill AI AttributeTypes:"
             << AttributeTypesToString(it->second);
    }
    if (base::optional_ref<const AutofillFormatString> format_string =
            field->format_string()) {
      std::string_view source;
      switch (field->format_string_source()) {
        case AutofillFormatStringSource::kUnset:
          source = "unset";
          break;
        case AutofillFormatStringSource::kHeuristics:
          source = "heuristic";
          break;
        case AutofillFormatStringSource::kModelResult:
          source = "model";
          break;
        case AutofillFormatStringSource::kServer:
          source = "server";
          break;
      }
      buffer << Tr{} << "Format string:"
             << base::StrCat({"\"", base::UTF16ToUTF8(format_string->value),
                              "\" from ", source});
    }
    buffer << Tr{} << "Section:" << field->section();

    constexpr size_t kMaxLabelSize = 100;
    const std::u16string& label = [&]() -> const std::u16string& {
      if (auto it = parseable_labels.find(field->global_id());
          it != parseable_labels.end()) {
        return it->second;
      }
      return field->label();
    }();
    const std::u16string truncated_label =
        label.substr(0, std::min(label.length(), kMaxLabelSize));
    buffer << Tr{} << "Label:" << truncated_label;

    buffer << Tr{} << "Is empty:" << ToYesOrNo(field->value().empty());
    buffer << Tr{} << "Is focusable:"
           << (field->IsFocusable() ? "Yes (focusable)" : "No (unfocusable)");
    buffer << Tr{} << "Is visible:"
           << (field->is_visible() ? "Yes (visible)" : "No (invisible)");
    buffer << Tr{} << "Ranks: "
           << base::StringPrintf(
                  "Field rank: %zu, rank in signature group: %zu, "
                  "field rank in host form: %zu, rank in host form signature "
                  "group: %zu",
                  field->rank(), field->rank_in_signature_group(),
                  field->rank_in_host_form(),
                  field->rank_in_host_form_signature_group());
    buffer << CTag{"table"};
    buffer << CTag{"td"};
    buffer << CTag{"tr"};
  }
  buffer << CTag{"table"};
  buffer << CTag{"div"};
  return buffer;
}

base::flat_map<FieldGlobalId, AutofillServerPrediction>
FormStructure::GetServerPredictions(
    const std::vector<FieldGlobalId>& field_ids) const {
  auto predictions = base::MakeFlatMap<FieldGlobalId, AutofillServerPrediction>(
      field_ids, {}, [](const FieldGlobalId& id) {
        return std::make_pair(id, AutofillServerPrediction());
      });
  for (const std::unique_ptr<AutofillField>& field : fields_) {
    auto field_in_predictions = predictions.find(field->global_id());
    if (field_in_predictions != predictions.end()) {
      field_in_predictions->second = AutofillServerPrediction(*field);
    }
  }
  return predictions;
}

base::flat_map<FieldGlobalId, FieldType> FormStructure::GetHeuristicPredictions(
    HeuristicSource source,
    const std::vector<FieldGlobalId>& field_ids) const {
  auto predictions = base::MakeFlatMap<FieldGlobalId, FieldType>(
      field_ids, {}, [](const FieldGlobalId& id) {
        return std::make_pair(id, NO_SERVER_DATA);
      });
  for (const std::unique_ptr<AutofillField>& field : fields_) {
    auto field_in_predictions = predictions.find(field->global_id());
    if (field_in_predictions != predictions.end()) {
      field_in_predictions->second = field->heuristic_type(source);
    }
  }
  return predictions;
}

}  // namespace autofill

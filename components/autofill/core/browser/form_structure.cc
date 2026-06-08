// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "components/autofill/core/browser/autofill_ai_form_rationalization.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_format_string.h"
#include "components/autofill/core/browser/autofill_server_prediction.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_processing/name_processing_util.h"
#include "components/autofill/core/browser/form_structure_rationalizer.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/language_code.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/origin.h"

namespace autofill {

using mojom::SubmissionIndicatorEvent;

namespace {

std::string ServerTypesToString(const AutofillField& field) {
  std::vector<std::string_view> server_types =
      base::ToVector(field.server_predictions(), [](const auto& prediction) {
        return FieldTypeToStringView(
            ToSafeFieldType(prediction.type()).value_or(NO_SERVER_DATA));
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

// Searches in `field_map` for a field matching `field_data` either by
// `FieldGlobalId` or uniquely by `FieldSignature`. Returns the found field and
// erases it from `field_map` if found, and `nullptr` otherwise.
std::unique_ptr<AutofillField> ExtractMatchingFieldToUpdate(
    const FormFieldData& field_data,
    std::map<FieldGlobalId, std::unique_ptr<AutofillField>>& field_map) {
  auto find_unique_signature_match = [&](FieldSignature signature) {
    auto pred = [&](const auto& p) {
      return p.second->GetFieldSignature() == signature;
    };
    auto it = std::ranges::find_if(field_map, pred);
    return it != field_map.end() &&
                   std::ranges::find_if(std::next(it), field_map.end(), pred) ==
                       field_map.end()
               ? it
               : field_map.end();
  };

  auto it = field_map.find(field_data.global_id());
  if (it == field_map.end()) {
    it = find_unique_signature_match(
        CalculateFieldSignatureForField(field_data));
  }
  if (it == field_map.end()) {
    return nullptr;
  }
  std::unique_ptr<AutofillField> field = std::move(it->second);
  field_map.erase(it);
  return field;
}

}  // namespace

FormStructure::AutofillFieldCopyableVector::AutofillFieldCopyableVector() =
    default;

FormStructure::AutofillFieldCopyableVector::AutofillFieldCopyableVector(
    const AutofillFieldCopyableVector& other) {
  data_ = base::ToVector(
      other.data_, [](const std::unique_ptr<AutofillField>& field) {
        return AutofillField::Clone(*field, AutofillFieldCopyKey());
      });
}

FormStructure::AutofillFieldCopyableVector::AutofillFieldCopyableVector(
    AutofillFieldCopyableVector&&) = default;

FormStructure::AutofillFieldCopyableVector&
FormStructure::AutofillFieldCopyableVector::operator=(
    AutofillFieldCopyableVector&&) = default;

FormStructure::AutofillFieldCopyableVector&
FormStructure::AutofillFieldCopyableVector::operator=(
    const AutofillFieldCopyableVector& other) {
  if (this == &other) {
    return *this;
  }
  data_ = base::ToVector(
      other.data_, [](const std::unique_ptr<AutofillField>& field) {
        return AutofillField::Clone(*field, AutofillFieldCopyKey());
      });
  return *this;
}

FormStructure::AutofillFieldCopyableVector::~AutofillFieldCopyableVector() =
    default;

FormStructure::FormStructure(const FormData& form)
    : form_parsed_timestamp_(base::TimeTicks::Now()),
      host_frame_(form.host_frame()),
      renderer_id_(form.renderer_id()) {
  UpdateFormData(form);
}

FormStructure::FormStructure(
    FormSignature form_signature,
    const std::vector<FieldSignature>& field_signatures)
    : form_signature_(form_signature) {
  for (const FieldSignature& signature : field_signatures) {
    fields_.data().push_back(
        AutofillField::CreateForPasswordManagerUpload(signature));
  }
  DetermineFieldRanks();
}

FormStructure::FormStructure(const FormStructure&) = default;
FormStructure& FormStructure::operator=(const FormStructure&) = default;

FormStructure::~FormStructure() = default;

void FormStructure::DetermineFieldRanks() {
  size_t rank = 0;
  std::map<FormGlobalId, size_t> rank_in_host_form;
  std::map<FieldSignature, size_t> rank_in_signature_group;
  std::map<std::pair<FormGlobalId, FieldSignature>, size_t>
      rank_in_host_form_signature_group;

  for (std::unique_ptr<AutofillField>& field : fields_) {
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
  for (const std::unique_ptr<AutofillField>& field : fields_) {
    if (!section_id_map.contains(field->section())) {
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

  for (const std::unique_ptr<AutofillField>& field : fields_) {
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
    for (const std::unique_ptr<AutofillField>& field : form->fields_) {
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

void FormStructure::UpdateFormData(const FormData& form_data) {
  CHECK_EQ(form_data.global_id(), global_id());

  id_attribute_ = form_data.id_attribute();
  name_attribute_ = form_data.name_attribute();
  form_name_ = form_data.name();
  button_titles_ = form_data.button_titles();
  source_url_ = form_data.url();
  full_source_url_ = form_data.full_url();
  target_url_ = form_data.action();
  main_frame_origin_ = form_data.main_frame_origin();
  version_ = form_data.version();
  child_frames_ = form_data.child_frames();

  // No need to copy these members as it is assumed that
  // `this->global_id() == form_data.global_id()`.
  // host_frame_ = form_data.host_frame();
  // renderer_id_ = form_data.renderer_id();

  // TODO(crbug.com/456719060): Figure out whether those members should be
  // replicated in `FormStructure`.
  // form_data.is_gaia_with_skip_save_password_form();
  // form_data.likely_contains_captcha();
  // form_data.submission_event();
  // form_data.username_predictions();

  // This map will contain the Autofill[Type|PredictionSource] of the fields
  // in the outdated version of the form. They need to be cached and retrieved
  // later (See comment below on usage of the map).
  auto types_and_sources = base::MakeFlatMap<
      FieldGlobalId,
      std::pair<AutofillType, std::optional<AutofillPredictionSource>>>(
      fields_, /*comp=*/{}, [](const std::unique_ptr<AutofillField>& field) {
        return std::pair(field->global_id(),
                         std::pair(field->Type(), field->PredictionSource()));
      });

  std::map<FieldGlobalId, std::unique_ptr<AutofillField>> field_map;
  for (std::unique_ptr<AutofillField>& field : fields_) {
    field_map[field->global_id()] = std::move(field);
  }

  std::vector<std::unique_ptr<AutofillField>> fields =
      base::ToVector(form_data.fields(), [&](const FormFieldData& field_data) {
        std::unique_ptr<AutofillField> autofill_field =
            ExtractMatchingFieldToUpdate(field_data, field_map);
        if (!autofill_field) {
          // The field was newly added to the form, create an `AutofillField`
          // from it and add it to the list.
          return std::make_unique<AutofillField>(field_data);
        }
        // The field existed in the cache previously, update the cached members
        // of `FormFieldData` in `autofill_field` provided by `field_data`.
        autofill_field->UpdateFieldData(field_data, /*pass_key=*/{});
        return autofill_field;
      });

  fields_.data() = std::move(fields);

  form_signature_ = CalculateFormSignature(form_data);
  alternative_form_signature_ = CalculateAlternativeFormSignature(form_data);
  structural_form_signature_ = CalculateStructuralFormSignature(form_data);
  // Do further processing on the fields, as needed.
  SetFieldTypesFromAutocompleteAttribute();
  DetermineFieldRanks();

  for (const std::unique_ptr<AutofillField>& field : fields_) {
    if (const std::pair<AutofillType, std::optional<AutofillPredictionSource>>*
            type_and_source =
                base::FindOrNull(types_and_sources, field->global_id())) {
      const auto& [type, source] = *type_and_source;
      // `AutofillField::overall_type_` can become invalidated between the time
      // of populating fields and reaching this block (e.g.,
      // `FormStructure::SetFieldTypesFromAutocompleteAttribute()` calls
      // `AutofillField::SetHtmlType()` and that function resets it).
      //
      // Therefore it is important to maintain the same field type that was
      // present beforehand, otherwise the recomputation of
      // `AutofillField::overall_type_` would happen at the next call of
      // `AutofillField::Type()`, which would do so without accounting for
      // rationalization.
      field->SetTypeTo(type, source);
    }
  }
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
  return fields_.data()[index].get();
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
  data.set_fields(base::ToVector(
      fields_, [](const auto& field) -> FormFieldData { return *field; }));
  return data;
}

DenseSet<FormType> FormStructure::GetFormTypes(
    AutocompleteUnrecognizedBehavior ac_unrecognized_behavior) const {
  DenseSet<FormType> form_types;
  for (const auto& field : fields_) {
    if (field->ShouldSuppressSuggestionsAndFillingByDefault(
            ac_unrecognized_behavior)) {
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
  buffer << "\n Form alternative signature: "
         << base::StrCat({base::NumberToString(
                              form.alternative_form_signature().value()),
                          " - ",
                          base::NumberToString(HashFormSignature(
                              form.alternative_form_signature()))});
  buffer << "\n Form structural signature: "
         << base::StrCat(
                {base::NumberToString(form.structural_form_signature().value()),
                 " - ",
                 base::NumberToString(
                     HashFormSignature(form.structural_form_signature()))});
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
  buffer << Tr{} << "Form structural signature:"
         << base::StrCat(
                {base::NumberToString(form.structural_form_signature().value()),
                 " - ",
                 base::NumberToString(
                     HashFormSignature(form.structural_form_signature()))});
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
    const std::u16string& label = field->label();
    const std::u16string truncated_label =
        label.substr(0, std::min(label.length(), kMaxLabelSize));
    buffer << Tr{} << "Label:" << truncated_label;

    buffer << Tr{} << "Is empty:" << ToYesOrNo(field->value().empty());
    buffer << Tr{} << "Is focusable:"
           << (field->is_focusable() ? "Yes (focusable)" : "No (unfocusable)");
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
    base::span<const FieldGlobalId> field_ids) const {
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
    base::span<const FieldGlobalId> field_ids) const {
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

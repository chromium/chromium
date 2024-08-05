// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_MOJOM_AUTOFILL_TYPES_MOJOM_TRAITS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_MOJOM_AUTOFILL_TYPES_MOJOM_TRAITS_H_

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "mojo/public/cpp/base/text_direction_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/unguessable_token.mojom-shared.h"
#include "ui/gfx/geometry/rect_f.h"

namespace mojo {

template <>
struct StructTraits<autofill::mojom::FrameTokenDataView, autofill::FrameToken> {
  static const base::UnguessableToken& token(const autofill::FrameToken& r) {
    return absl::visit([](const auto& t) -> const auto& { return t.value(); },
                       r);
  }

  static bool is_local(const autofill::FrameToken& r) {
    return absl::holds_alternative<autofill::LocalFrameToken>(r);
  }

  static bool Read(autofill::mojom::FrameTokenDataView data,
                   autofill::FrameToken* out);
};

template <>
struct StructTraits<autofill::mojom::FrameTokenWithPredecessorDataView,
                    autofill::FrameTokenWithPredecessor> {
  static const autofill::FrameToken& token(
      const autofill::FrameTokenWithPredecessor& r) {
    return r.token;
  }

  static int predecessor(const autofill::FrameTokenWithPredecessor& r) {
    return r.predecessor;
  }

  static bool Read(autofill::mojom::FrameTokenWithPredecessorDataView data,
                   autofill::FrameTokenWithPredecessor* out);
};

template <>
struct StructTraits<autofill::mojom::FormRendererIdDataView,
                    autofill::FormRendererId> {
  static uint64_t id(autofill::FormRendererId r) { return r.value(); }

  static bool Read(autofill::mojom::FormRendererIdDataView data,
                   autofill::FormRendererId* out);
};

template <>
struct StructTraits<autofill::mojom::FieldRendererIdDataView,
                    autofill::FieldRendererId> {
  static uint64_t id(autofill::FieldRendererId r) { return r.value(); }

  static bool Read(autofill::mojom::FieldRendererIdDataView data,
                   autofill::FieldRendererId* out);
};

template <>
struct StructTraits<autofill::mojom::SelectOptionDataView,
                    autofill::SelectOption> {
  static const std::u16string& value(const autofill::SelectOption& r) {
    return r.value;
  }

  static const std::u16string& text(const autofill::SelectOption& r) {
    return r.text;
  }

  static bool Read(autofill::mojom::SelectOptionDataView data,
                   autofill::SelectOption* out);
};

template <>
struct UnionTraits<autofill::mojom::SectionValueDataView,
                   autofill::Section::SectionValue> {
  static autofill::mojom::SectionValueDataView::Tag GetTag(
      const autofill::Section::SectionValue& r);

  static bool default_section(const autofill::Section::SectionValue& r) {
    DCHECK(absl::holds_alternative<autofill::Section::Default>(r));
    return true;
  }

  static const autofill::Section::Autocomplete& autocomplete(
      const autofill::Section::SectionValue& r) {
    return absl::get<autofill::Section::Autocomplete>(r);
  }

  static const autofill::Section::FieldIdentifier& field_identifier(
      const autofill::Section::SectionValue& r) {
    return absl::get<autofill::Section::FieldIdentifier>(r);
  }

  static bool Read(autofill::mojom::SectionValueDataView data,
                   autofill::Section::SectionValue* out);
};

template <>
struct StructTraits<autofill::mojom::SectionAutocompleteDataView,
                    autofill::Section::Autocomplete> {
  static const std::string& section(const autofill::Section::Autocomplete& r) {
    return r.section;
  }

  static autofill::mojom::HtmlFieldMode html_field_mode(
      const autofill::Section::Autocomplete& r) {
    return r.mode;
  }

  static bool Read(autofill::mojom::SectionAutocompleteDataView data,
                   autofill::Section::Autocomplete* out);
};

template <>
struct StructTraits<autofill::mojom::SectionFieldIdentifierDataView,
                    autofill::Section::FieldIdentifier> {
  static const std::string& field_name(
      const autofill::Section::FieldIdentifier& r) {
    return r.field_name;
  }

  static size_t local_frame_id(const autofill::Section::FieldIdentifier& r) {
    return r.local_frame_id;
  }

  static autofill::FieldRendererId field_renderer_id(
      const autofill::Section::FieldIdentifier& r) {
    return r.field_renderer_id;
  }

  static bool Read(autofill::mojom::SectionFieldIdentifierDataView data,
                   autofill::Section::FieldIdentifier* out);
};

template <>
struct StructTraits<autofill::mojom::SectionDataView, autofill::Section> {
  static const autofill::Section::SectionValue& value(
      const autofill::Section& r) {
    return r.value_;
  }

  static bool Read(autofill::mojom::SectionDataView data,
                   autofill::Section* out);
};

template <>
struct StructTraits<autofill::mojom::AutocompleteParsingResultDataView,
                    autofill::AutocompleteParsingResult> {
  static const std::string& section(
      const autofill::AutocompleteParsingResult& r) {
    return r.section;
  }

  static autofill::mojom::HtmlFieldMode mode(
      const autofill::AutocompleteParsingResult& r) {
    return r.mode;
  }

  static autofill::mojom::HtmlFieldType field_type(
      const autofill::AutocompleteParsingResult& r) {
    return r.field_type;
  }

  static bool webauthn(const autofill::AutocompleteParsingResult& r) {
    return r.webauthn;
  }

  static bool Read(autofill::mojom::AutocompleteParsingResultDataView data,
                   autofill::AutocompleteParsingResult* out);
};

template <>
struct StructTraits<autofill::mojom::FormFieldDataDataView,
                    autofill::FormFieldData> {
  static const std::u16string& label(const autofill::FormFieldData& r) {
    return r.label();
  }

  static const std::u16string& name(const autofill::FormFieldData& r) {
    return r.name();
  }

  static const std::u16string& id_attribute(const autofill::FormFieldData& r) {
    return r.id_attribute();
  }

  static const std::u16string& name_attribute(
      const autofill::FormFieldData& r) {
    return r.name_attribute();
  }

  static const std::u16string& value(const autofill::FormFieldData& r) {
    return r.value();
  }

  static const std::u16string& selected_text(const autofill::FormFieldData& r) {
    return r.selected_text();
  }

  static autofill::mojom::FormControlType form_control_type(
      const autofill::FormFieldData& r) {
    return r.form_control_type();
  }

  static const std::string& autocomplete_attribute(
      const autofill::FormFieldData& r) {
    return r.autocomplete_attribute();
  }

  static const std::optional<autofill::AutocompleteParsingResult>&
  parsed_autocomplete(const autofill::FormFieldData& r) {
    return r.parsed_autocomplete();
  }

  static const std::u16string& placeholder(const autofill::FormFieldData& r) {
    return r.placeholder();
  }

  static const std::u16string& css_classes(const autofill::FormFieldData& r) {
    return r.css_classes();
  }

  static const std::u16string& aria_label(const autofill::FormFieldData& r) {
    return r.aria_label();
  }

  static const std::u16string& aria_description(
      const autofill::FormFieldData& r) {
    return r.aria_description();
  }

  static autofill::FieldRendererId renderer_id(
      const autofill::FormFieldData& r) {
    return r.renderer_id();
  }

  static autofill::FormRendererId host_form_id(
      const autofill::FormFieldData& r) {
    return r.host_form_id();
  }

  static uint32_t properties_mask(const autofill::FormFieldData& r) {
    return r.properties_mask();
  }

  static int32_t form_control_ax_id(const autofill::FormFieldData& r) {
    return r.form_control_ax_id();
  }

  static uint64_t max_length(const autofill::FormFieldData& r) {
    return r.max_length();
  }

  static bool is_user_edited(const autofill::FormFieldData& r) {
    return r.is_user_edited();
  }

  static bool is_autofilled(const autofill::FormFieldData& r) {
    return r.is_autofilled();
  }

  static const autofill::Section& section(const autofill::FormFieldData& r) {
    return r.section();
  }

  static autofill::FormFieldData::CheckStatus check_status(
      const autofill::FormFieldData& r) {
    return r.check_status();
  }

  static bool is_focusable(const autofill::FormFieldData& r) {
    return r.is_focusable();
  }

  static bool is_visible(const autofill::FormFieldData& r) {
    return r.is_visible();
  }

  static bool should_autocomplete(const autofill::FormFieldData& r) {
    return r.should_autocomplete();
  }

  static autofill::FormFieldData::RoleAttribute role(
      const autofill::FormFieldData& r) {
    return r.role();
  }

  static base::i18n::TextDirection text_direction(
      const autofill::FormFieldData& r) {
    return r.text_direction();
  }

  static bool is_enabled(const autofill::FormFieldData& r) {
    return r.is_enabled();
  }

  static bool is_readonly(const autofill::FormFieldData& r) {
    return r.is_readonly();
  }

  static const std::u16string& user_input(const autofill::FormFieldData& r) {
    return r.user_input();
  }

  static bool allows_writing_suggestions(const autofill::FormFieldData& r) {
    return r.allows_writing_suggestions();
  }

  static const std::vector<autofill::SelectOption>& options(
      const autofill::FormFieldData& r) {
    return r.options();
  }

  static autofill::FormFieldData::LabelSource label_source(
      const autofill::FormFieldData& r) {
    return r.label_source();
  }

  static const gfx::RectF& bounds(const autofill::FormFieldData& r) {
    return r.bounds();
  }

  static const std::vector<autofill::SelectOption>& datalist_options(
      const autofill::FormFieldData& r) {
    return r.datalist_options();
  }

  static bool Read(autofill::mojom::FormFieldDataDataView data,
                   autofill::FormFieldData* out);

  static bool force_override(const autofill::FormFieldData& r) {
    return r.force_override();
  }
};

template <>
struct StructTraits<autofill::mojom::FormFieldData_FillDataDataView,
                    autofill::FormFieldData::FillData> {
  static const std::u16string& value(
      const autofill::FormFieldData::FillData& r) {
    return r.value;
  }

  static autofill::FieldRendererId renderer_id(
      const autofill::FormFieldData::FillData& r) {
    return r.renderer_id;
  }

  static autofill::FormRendererId host_form_id(
      const autofill::FormFieldData::FillData& r) {
    return r.host_form_id;
  }

  static bool is_autofilled(const autofill::FormFieldData::FillData& r) {
    return r.is_autofilled;
  }

  static bool Read(autofill::mojom::FormFieldData_FillDataDataView data,
                   autofill::FormFieldData::FillData* out);

  static bool force_override(const autofill::FormFieldData::FillData& r) {
    return r.force_override;
  }
};

template <>
struct StructTraits<autofill::mojom::ButtonTitleInfoDataView,
                    autofill::ButtonTitleInfo> {
  static const std::u16string& title(const autofill::ButtonTitleInfo& r) {
    return r.first;
  }

  static autofill::mojom::ButtonTitleType type(
      const autofill::ButtonTitleInfo& r) {
    return r.second;
  }

  static bool Read(autofill::mojom::ButtonTitleInfoDataView data,
                   autofill::ButtonTitleInfo* out);
};

template <>
struct StructTraits<autofill::mojom::FormDataDataView, autofill::FormData> {
  static const std::u16string& id_attribute(const autofill::FormData& r) {
    return r.id_attribute();
  }

  static const std::u16string& name_attribute(const autofill::FormData& r) {
    return r.name_attribute();
  }

  static const std::u16string& name(const autofill::FormData& r) {
    return r.name();
  }

  static const autofill::ButtonTitleList& button_titles(
      const autofill::FormData& r) {
    return r.button_titles();
  }

  static const GURL& action(const autofill::FormData& r) { return r.action(); }

  static bool is_action_empty(const autofill::FormData& r) {
    return r.is_action_empty();
  }

  static autofill::FormRendererId renderer_id(const autofill::FormData& r) {
    return r.renderer_id();
  }

  static const std::vector<autofill::FrameTokenWithPredecessor>& child_frames(
      const autofill::FormData& r) {
    return r.child_frames();
  }

  static autofill::mojom::SubmissionIndicatorEvent submission_event(
      const autofill::FormData& r) {
    return r.submission_event();
  }

  static const std::vector<autofill::FormFieldData>& fields(
      const autofill::FormData& r) {
    return r.fields();
  }

  static const std::vector<autofill::FieldRendererId>& username_predictions(
      const autofill::FormData& r) {
    return r.username_predictions();
  }

  static bool is_gaia_with_skip_save_password_form(
      const autofill::FormData& d) {
    return d.is_gaia_with_skip_save_password_form();
  }

  static bool likely_contains_captcha(const autofill::FormData& d) {
    return d.likely_contains_captcha();
  }

  static bool Read(autofill::mojom::FormDataDataView data,
                   autofill::FormData* out);
};

template <>
struct StructTraits<autofill::mojom::FormFieldDataPredictionsDataView,
                    autofill::FormFieldDataPredictions> {
  static const std::string& host_form_signature(
      const autofill::FormFieldDataPredictions& r) {
    return r.host_form_signature;
  }

  static const std::string& signature(
      const autofill::FormFieldDataPredictions& r) {
    return r.signature;
  }

  static const std::string& heuristic_type(
      const autofill::FormFieldDataPredictions& r) {
    return r.heuristic_type;
  }

  static const std::optional<std::string>& server_type(
      const autofill::FormFieldDataPredictions& r) {
    return r.server_type;
  }

  static const std::string& html_type(
      const autofill::FormFieldDataPredictions& r) {
    return r.html_type;
  }

  static const std::string& overall_type(
      const autofill::FormFieldDataPredictions& r) {
    return r.overall_type;
  }

  static const std::string& parseable_name(
      const autofill::FormFieldDataPredictions& r) {
    return r.parseable_name;
  }

  static const std::string& section(
      const autofill::FormFieldDataPredictions& r) {
    return r.section;
  }

  static size_t rank(const autofill::FormFieldDataPredictions& r) {
    return r.rank;
  }

  static size_t rank_in_signature_group(
      const autofill::FormFieldDataPredictions& r) {
    return r.rank_in_signature_group;
  }

  static size_t rank_in_host_form(const autofill::FormFieldDataPredictions& r) {
    return r.rank_in_host_form;
  }

  static size_t rank_in_host_form_signature_group(
      const autofill::FormFieldDataPredictions& r) {
    return r.rank_in_host_form_signature_group;
  }

  static bool Read(autofill::mojom::FormFieldDataPredictionsDataView data,
                   autofill::FormFieldDataPredictions* out);
};

template <>
struct StructTraits<autofill::mojom::FormDataPredictionsDataView,
                    autofill::FormDataPredictions> {
  static const autofill::FormData& data(
      const autofill::FormDataPredictions& r) {
    return r.data;
  }

  static const std::string& signature(const autofill::FormDataPredictions& r) {
    return r.signature;
  }

  static const std::string& alternative_signature(
      const autofill::FormDataPredictions& r) {
    return r.alternative_signature;
  }

  static const std::vector<autofill::FormFieldDataPredictions>& fields(
      const autofill::FormDataPredictions& r) {
    return r.fields;
  }

  static bool Read(autofill::mojom::FormDataPredictionsDataView data,
                   autofill::FormDataPredictions* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordAndMetadataDataView,
                    autofill::PasswordAndMetadata> {
  static const std::u16string& username_value(
      const autofill::PasswordAndMetadata& r) {
    return r.username_value;
  }

  static const std::u16string& password_value(
      const autofill::PasswordAndMetadata& r) {
    return r.password_value;
  }

  static const std::string& realm(const autofill::PasswordAndMetadata& r) {
    return r.realm;
  }

  static bool uses_account_store(const autofill::PasswordAndMetadata& r) {
    return r.uses_account_store;
  }

  static bool Read(autofill::mojom::PasswordAndMetadataDataView data,
                   autofill::PasswordAndMetadata* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordFormFillDataDataView,
                    autofill::PasswordFormFillData> {
  static autofill::FormRendererId form_renderer_id(
      const autofill::PasswordFormFillData& r) {
    return r.form_renderer_id;
  }

  static const GURL& url(const autofill::PasswordFormFillData& r) {
    return r.url;
  }

  static const autofill::FieldRendererId& username_element_renderer_id(
      const autofill::PasswordFormFillData& r) {
    return r.username_element_renderer_id;
  }

  static const autofill::FieldRendererId& password_element_renderer_id(
      const autofill::PasswordFormFillData& r) {
    return r.password_element_renderer_id;
  }

  static bool username_may_use_prefilled_placeholder(
      const autofill::PasswordFormFillData& r) {
    return r.username_may_use_prefilled_placeholder;
  }

  static const autofill::PasswordAndMetadata& preferred_login(
      const autofill::PasswordFormFillData& r) {
    return r.preferred_login;
  }

  static const autofill::PasswordFormFillData::LoginCollection&
  additional_logins(const autofill::PasswordFormFillData& r) {
    return r.additional_logins;
  }

  static bool wait_for_username(const autofill::PasswordFormFillData& r) {
    return r.wait_for_username;
  }

  static std::vector<autofill::FieldRendererId> suggestion_banned_fields(
      const autofill::PasswordFormFillData& r) {
    return r.suggestion_banned_fields;
  }

  static bool Read(autofill::mojom::PasswordFormFillDataDataView data,
                   autofill::PasswordFormFillData* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordFormGenerationDataDataView,
                    autofill::PasswordFormGenerationData> {
  static autofill::FieldRendererId new_password_renderer_id(
      const autofill::PasswordFormGenerationData& r) {
    return r.new_password_renderer_id;
  }

  static autofill::FieldRendererId confirmation_password_renderer_id(
      const autofill::PasswordFormGenerationData& r) {
    return r.confirmation_password_renderer_id;
  }

  static bool Read(autofill::mojom::PasswordFormGenerationDataDataView data,
                   autofill::PasswordFormGenerationData* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordGenerationUIDataDataView,
                    autofill::password_generation::PasswordGenerationUIData> {
  static const gfx::RectF& bounds(
      const autofill::password_generation::PasswordGenerationUIData& r) {
    return r.bounds;
  }

  static int max_length(
      const autofill::password_generation::PasswordGenerationUIData& r) {
    return r.max_length;
  }

  static const std::u16string& generation_element(
      const autofill::password_generation::PasswordGenerationUIData& r) {
    return r.generation_element;
  }

  static autofill::FieldRendererId generation_element_id(
      const autofill::password_generation::PasswordGenerationUIData& r) {
    return r.generation_element_id;
  }

  static bool is_generation_element_password_type(
      const autofill::password_generation::PasswordGenerationUIData& r) {
    return r.is_generation_element_password_type;
  }

  static base::i18n::TextDirection text_direction(
      const autofill::password_generation::PasswordGenerationUIData& r) {
    return r.text_direction;
  }

  static const autofill::FormData& form_data(
      const autofill::password_generation::PasswordGenerationUIData& r) {
    return r.form_data;
  }

  static bool input_field_empty(
      const autofill::password_generation::PasswordGenerationUIData& r) {
    return r.input_field_empty;
  }

  static bool Read(
      autofill::mojom::PasswordGenerationUIDataDataView data,
      autofill::password_generation::PasswordGenerationUIData* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordSuggestionRequestDataView,
                    autofill::PasswordSuggestionRequest> {
  static autofill::FieldRendererId element_id(
      const autofill::PasswordSuggestionRequest& r) {
    return r.element_id;
  }

  static const autofill::FormData& form_data(
      const autofill::PasswordSuggestionRequest& r) {
    return r.form_data;
  }

  static autofill::AutofillSuggestionTriggerSource trigger_source(
      const autofill::PasswordSuggestionRequest& r) {
    return r.trigger_source;
  }

  static uint64_t username_field_index(
      const autofill::PasswordSuggestionRequest& r) {
    return r.username_field_index;
  }

  static uint64_t password_field_index(
      const autofill::PasswordSuggestionRequest& r) {
    return r.password_field_index;
  }

  static base::i18n::TextDirection text_direction(
      const autofill::PasswordSuggestionRequest& r) {
    return r.text_direction;
  }

  static const std::u16string& typed_username(
      const autofill::PasswordSuggestionRequest& r) {
    return r.typed_username;
  }

  static int show_webauthn_credentials(
      const autofill::PasswordSuggestionRequest& r) {
    return r.show_webauthn_credentials;
  }

  static const gfx::RectF& bounds(
      const autofill::PasswordSuggestionRequest& r) {
    return r.bounds;
  }

  static bool Read(autofill::mojom::PasswordSuggestionRequestDataView data,
                   autofill::PasswordSuggestionRequest* out);
};

template <>
struct StructTraits<autofill::mojom::ParsingResultDataView,
                    autofill::ParsingResult> {
  static autofill::FieldRendererId username_renderer_id(
      const autofill::ParsingResult& r) {
    return r.username_renderer_id;
  }

  static autofill::FieldRendererId password_renderer_id(
      const autofill::ParsingResult& r) {
    return r.password_renderer_id;
  }

  static autofill::FieldRendererId new_password_renderer_id(
      const autofill::ParsingResult& r) {
    return r.new_password_renderer_id;
  }

  static autofill::FieldRendererId confirm_password_renderer_id(
      const autofill::ParsingResult& r) {
    return r.confirm_password_renderer_id;
  }

  static bool Read(autofill::mojom::ParsingResultDataView data,
                   autofill::ParsingResult* out);
};

}  // namespace mojo

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_MOJOM_AUTOFILL_TYPES_MOJOM_TRAITS_H_

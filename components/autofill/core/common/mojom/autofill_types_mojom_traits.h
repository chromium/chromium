// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_MOJOM_AUTOFILL_TYPES_MOJOM_TRAITS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_MOJOM_AUTOFILL_TYPES_MOJOM_TRAITS_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "mojo/public/cpp/base/text_direction_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/origin.h"

namespace mojo {

template <>
struct StructTraits<autofill::mojom::FormFieldDataDataView,
                    autofill::FormFieldData> {
  static const base::string16& label(const autofill::FormFieldData& r) {
    return r.label;
  }

  static const base::string16& name(const autofill::FormFieldData& r) {
    return r.name;
  }

  static const base::string16& id_attribute(const autofill::FormFieldData& r) {
    return r.id_attribute;
  }

  static const base::string16& name_attribute(
      const autofill::FormFieldData& r) {
    return r.name_attribute;
  }

  static const base::string16& value(const autofill::FormFieldData& r) {
    return r.value;
  }

  static const std::string& form_control_type(
      const autofill::FormFieldData& r) {
    return r.form_control_type;
  }

  static const std::string& autocomplete_attribute(
      const autofill::FormFieldData& r) {
    return r.autocomplete_attribute;
  }

  static const base::string16& placeholder(const autofill::FormFieldData& r) {
    return r.placeholder;
  }

  static const base::string16& css_classes(const autofill::FormFieldData& r) {
    return r.css_classes;
  }

  static const base::string16& aria_label(const autofill::FormFieldData& r) {
    return r.aria_label;
  }

  static const base::string16& aria_description(
      const autofill::FormFieldData& r) {
    return r.aria_description;
  }

  static uint32_t unique_renderer_id(const autofill::FormFieldData& r) {
    return r.unique_renderer_id;
  }

  static uint32_t properties_mask(const autofill::FormFieldData& r) {
    return r.properties_mask;
  }

  static int32_t form_control_ax_id(const autofill::FormFieldData& r) {
    return r.form_control_ax_id;
  }

  static uint64_t max_length(const autofill::FormFieldData& r) {
    return r.max_length;
  }

  static bool is_autofilled(const autofill::FormFieldData& r) {
    return r.is_autofilled;
  }

  static const std::string& section(const autofill::FormFieldData& r) {
    return r.section;
  }

  static autofill::FormFieldData::CheckStatus check_status(
      const autofill::FormFieldData& r) {
    return r.check_status;
  }

  static bool is_focusable(const autofill::FormFieldData& r) {
    return r.is_focusable;
  }

  static bool should_autocomplete(const autofill::FormFieldData& r) {
    return r.should_autocomplete;
  }

  static autofill::FormFieldData::RoleAttribute role(
      const autofill::FormFieldData& r) {
    return r.role;
  }

  static base::i18n::TextDirection text_direction(
      const autofill::FormFieldData& r) {
    return r.text_direction;
  }

  static bool is_enabled(const autofill::FormFieldData& r) {
    return r.is_enabled;
  }

  static bool is_readonly(const autofill::FormFieldData& r) {
    return r.is_readonly;
  }

  static const base::string16& typed_value(const autofill::FormFieldData& r) {
    return r.typed_value;
  }

  static const std::vector<base::string16>& option_values(
      const autofill::FormFieldData& r) {
    return r.option_values;
  }

  static const std::vector<base::string16>& option_contents(
      const autofill::FormFieldData& r) {
    return r.option_contents;
  }

  static autofill::FormFieldData::LabelSource label_source(
      const autofill::FormFieldData& r) {
    return r.label_source;
  }

  static bool Read(autofill::mojom::FormFieldDataDataView data,
                   autofill::FormFieldData* out);
};

template <>
struct StructTraits<autofill::mojom::ButtonTitleInfoDataView,
                    autofill::ButtonTitleInfo> {
  static const base::string16& title(const autofill::ButtonTitleInfo& r) {
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
  static const base::string16& id_attribute(const autofill::FormData& r) {
    return r.id_attribute;
  }

  static const base::string16& name_attribute(const autofill::FormData& r) {
    return r.name_attribute;
  }

  static const base::string16& name(const autofill::FormData& r) {
    return r.name;
  }

  static const autofill::ButtonTitleList& button_titles(
      const autofill::FormData& r) {
    return r.button_titles;
  }

  static const GURL& url(const autofill::FormData& r) { return r.url; }

  static const GURL& action(const autofill::FormData& r) { return r.action; }

  static bool is_action_empty(const autofill::FormData& r) {
    return r.is_action_empty;
  }

  static const url::Origin& main_frame_origin(const autofill::FormData& r) {
    return r.main_frame_origin;
  }

  static bool is_form_tag(const autofill::FormData& r) { return r.is_form_tag; }

  static bool is_formless_checkout(const autofill::FormData& r) {
    return r.is_formless_checkout;
  }

  static uint32_t unique_renderer_id(const autofill::FormData& r) {
    return r.unique_renderer_id;
  }

  static autofill::mojom::SubmissionIndicatorEvent submission_event(
      const autofill::FormData& r) {
    return r.submission_event;
  }

  static const std::vector<autofill::FormFieldData>& fields(
      const autofill::FormData& r) {
    return r.fields;
  }

  static const std::vector<uint32_t>& username_predictions(
      const autofill::FormData& r) {
    return r.username_predictions;
  }

  static bool is_gaia_with_skip_save_password_form(
      const autofill::FormData& d) {
    return d.is_gaia_with_skip_save_password_form;
  }

  static bool Read(autofill::mojom::FormDataDataView data,
                   autofill::FormData* out);
};

template <>
struct StructTraits<autofill::mojom::FormFieldDataPredictionsDataView,
                    autofill::FormFieldDataPredictions> {
  static const autofill::FormFieldData& field(
      const autofill::FormFieldDataPredictions& r) {
    return r.field;
  }

  static const std::string& signature(
      const autofill::FormFieldDataPredictions& r) {
    return r.signature;
  }

  static const std::string& heuristic_type(
      const autofill::FormFieldDataPredictions& r) {
    return r.heuristic_type;
  }

  static const std::string& server_type(
      const autofill::FormFieldDataPredictions& r) {
    return r.server_type;
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
  static const base::string16& password(
      const autofill::PasswordAndMetadata& r) {
    return r.password;
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
  static uint32_t form_renderer_id(const autofill::PasswordFormFillData& r) {
    return r.form_renderer_id;
  }

  static const GURL& origin(const autofill::PasswordFormFillData& r) {
    return r.origin;
  }

  static const GURL& action(const autofill::PasswordFormFillData& r) {
    return r.action;
  }

  static const autofill::FormFieldData& username_field(
      const autofill::PasswordFormFillData& r) {
    return r.username_field;
  }

  static const autofill::FormFieldData& password_field(
      const autofill::PasswordFormFillData& r) {
    return r.password_field;
  }

  static bool username_may_use_prefilled_placeholder(
      const autofill::PasswordFormFillData& r) {
    return r.username_may_use_prefilled_placeholder;
  }

  static const std::string& preferred_realm(
      const autofill::PasswordFormFillData& r) {
    return r.preferred_realm;
  }

  static bool uses_account_store(const autofill::PasswordFormFillData& r) {
    return r.uses_account_store;
  }

  static const std::map<base::string16, autofill::PasswordAndMetadata>&
  additional_logins(const autofill::PasswordFormFillData& r) {
    return r.additional_logins;
  }

  static bool wait_for_username(const autofill::PasswordFormFillData& r) {
    return r.wait_for_username;
  }

  static bool has_renderer_ids(const autofill::PasswordFormFillData& r) {
    return r.has_renderer_ids;
  }

  static bool Read(autofill::mojom::PasswordFormFillDataDataView data,
                   autofill::PasswordFormFillData* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordFormGenerationDataDataView,
                    autofill::PasswordFormGenerationData> {
  static uint32_t new_password_renderer_id(
      const autofill::PasswordFormGenerationData& r) {
    return r.new_password_renderer_id;
  }

  static uint32_t confirmation_password_renderer_id(
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

  static const base::string16& generation_element(
      const autofill::password_generation::PasswordGenerationUIData& r) {
    return r.generation_element;
  }

  static uint32_t generation_element_id(
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

  static const autofill::PasswordForm& password_form(
      const autofill::password_generation::PasswordGenerationUIData& r) {
    return r.password_form;
  }

  static bool Read(
      autofill::mojom::PasswordGenerationUIDataDataView data,
      autofill::password_generation::PasswordGenerationUIData* out);
};

template <>
struct StructTraits<autofill::mojom::PasswordFormDataView,
                    autofill::PasswordForm> {
  static autofill::PasswordForm::Scheme scheme(
      const autofill::PasswordForm& r) {
    return r.scheme;
  }

  static const std::string& signon_realm(const autofill::PasswordForm& r) {
    return r.signon_realm;
  }

  static const GURL& origin_with_path(const autofill::PasswordForm& r) {
    return r.origin;
  }

  static const GURL& action(const autofill::PasswordForm& r) {
    return r.action;
  }

  static const std::string& affiliated_web_realm(
      const autofill::PasswordForm& r) {
    return r.affiliated_web_realm;
  }

  static const base::string16& submit_element(const autofill::PasswordForm& r) {
    return r.submit_element;
  }

  static const base::string16& username_element(
      const autofill::PasswordForm& r) {
    return r.username_element;
  }

  static bool username_marked_by_site(const autofill::PasswordForm& r) {
    return r.username_marked_by_site;
  }

  static const base::string16& username_value(const autofill::PasswordForm& r) {
    return r.username_value;
  }

  static const std::vector<autofill::ValueElementPair>& all_possible_usernames(
      const autofill::PasswordForm& r) {
    return r.all_possible_usernames;
  }

  static const std::vector<autofill::ValueElementPair>& all_possible_passwords(
      const autofill::PasswordForm& r) {
    return r.all_possible_passwords;
  }

  static bool form_has_autofilled_value(const autofill::PasswordForm& r) {
    return r.form_has_autofilled_value;
  }

  static const base::string16& password_element(
      const autofill::PasswordForm& r) {
    return r.password_element;
  }

  static const base::string16& password_value(const autofill::PasswordForm& r) {
    return r.password_value;
  }

  static const base::string16& new_password_element(
      const autofill::PasswordForm& r) {
    return r.new_password_element;
  }

  static const base::string16& new_password_value(
      const autofill::PasswordForm& r) {
    return r.new_password_value;
  }

  static bool new_password_marked_by_site(const autofill::PasswordForm& r) {
    return r.new_password_marked_by_site;
  }

  static const base::string16& confirmation_password_element(
      const autofill::PasswordForm& r) {
    return r.confirmation_password_element;
  }

  static bool preferred(const autofill::PasswordForm& r) { return r.preferred; }

  static const base::Time& date_created(const autofill::PasswordForm& r) {
    return r.date_created;
  }

  static const base::Time& date_synced(const autofill::PasswordForm& r) {
    return r.date_synced;
  }

  static bool blacklisted_by_user(const autofill::PasswordForm& r) {
    return r.blacklisted_by_user;
  }

  static autofill::PasswordForm::Type type(const autofill::PasswordForm& r) {
    return r.type;
  }

  static int32_t times_used(const autofill::PasswordForm& r) {
    return r.times_used;
  }

  static const autofill::FormData& form_data(const autofill::PasswordForm& r) {
    return r.form_data;
  }

  static autofill::PasswordForm::GenerationUploadStatus
  generation_upload_status(const autofill::PasswordForm& r) {
    return r.generation_upload_status;
  }

  static const base::string16& display_name(const autofill::PasswordForm& r) {
    return r.display_name;
  }

  static const GURL& icon_url(const autofill::PasswordForm& r) {
    return r.icon_url;
  }

  static const url::Origin& federation_origin(const autofill::PasswordForm& r) {
    return r.federation_origin;
  }

  static bool skip_zero_click(const autofill::PasswordForm& r) {
    return r.skip_zero_click;
  }

  static bool was_parsed_using_autofill_predictions(
      const autofill::PasswordForm& r) {
    return r.was_parsed_using_autofill_predictions;
  }

  static bool is_public_suffix_match(const autofill::PasswordForm& r) {
    return r.is_public_suffix_match;
  }

  static bool is_affiliation_based_match(const autofill::PasswordForm& r) {
    return r.is_affiliation_based_match;
  }

  static autofill::mojom::SubmissionIndicatorEvent submission_event(
      const autofill::PasswordForm& r) {
    return r.submission_event;
  }

  static bool only_for_fallback(const autofill::PasswordForm& r) {
    return r.only_for_fallback;
  }

  static bool Read(autofill::mojom::PasswordFormDataView data,
                   autofill::PasswordForm* out);
};

template <>
struct StructTraits<autofill::mojom::ValueElementPairDataView,
                    autofill::ValueElementPair> {
  static base::string16 value(const autofill::ValueElementPair& r) {
    return r.first;
  }

  static base::string16 field_name(const autofill::ValueElementPair& r) {
    return r.second;
  }

  static bool Read(autofill::mojom::ValueElementPairDataView data,
                   autofill::ValueElementPair* out);
};

template <>
struct StructTraits<autofill::mojom::ParsingResultDataView,
                    autofill::ParsingResult> {
  static uint32_t username_renderer_id(const autofill::ParsingResult& r) {
    return r.username_renderer_id;
  }

  static uint32_t password_renderer_id(const autofill::ParsingResult& r) {
    return r.password_renderer_id;
  }

  static uint32_t new_password_renderer_id(const autofill::ParsingResult& r) {
    return r.new_password_renderer_id;
  }

  static uint32_t confirm_password_renderer_id(
      const autofill::ParsingResult& r) {
    return r.confirm_password_renderer_id;
  }

  static bool Read(autofill::mojom::ParsingResultDataView data,
                   autofill::ParsingResult* out);
};

}  // namespace mojo

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_MOJOM_AUTOFILL_TYPES_MOJOM_TRAITS_H_

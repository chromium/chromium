// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/mojom/autofill_types_mojom_traits.h"

#include "base/i18n/rtl.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<autofill::mojom::FormRendererIdDataView,
                  autofill::FormRendererId>::
    Read(autofill::mojom::FormRendererIdDataView data,
         autofill::FormRendererId* out) {
  *out = autofill::FormRendererId(data.id());
  return true;
}

// static
bool StructTraits<autofill::mojom::FieldRendererIdDataView,
                  autofill::FieldRendererId>::
    Read(autofill::mojom::FieldRendererIdDataView data,
         autofill::FieldRendererId* out) {
  *out = autofill::FieldRendererId(data.id());
  return true;
}

// static
bool StructTraits<
    autofill::mojom::FormFieldDataDataView,
    autofill::FormFieldData>::Read(autofill::mojom::FormFieldDataDataView data,
                                   autofill::FormFieldData* out) {
  if (!data.ReadLabel(&out->label))
    return false;
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadIdAttribute(&out->id_attribute))
    return false;
  if (!data.ReadNameAttribute(&out->name_attribute))
    return false;
  if (!data.ReadValue(&out->value))
    return false;

  if (!data.ReadFormControlType(&out->form_control_type))
    return false;
  if (!data.ReadAutocompleteAttribute(&out->autocomplete_attribute))
    return false;

  if (!data.ReadPlaceholder(&out->placeholder))
    return false;

  if (!data.ReadCssClasses(&out->css_classes))
    return false;

  if (!data.ReadAriaLabel(&out->aria_label))
    return false;

  if (!data.ReadAriaDescription(&out->aria_description))
    return false;

  if (!data.ReadSection(&out->section))
    return false;

  out->properties_mask = data.properties_mask();

  if (!data.ReadUniqueRendererId(&out->unique_renderer_id))
    return false;

  out->form_control_ax_id = data.form_control_ax_id();
  out->max_length = data.max_length();
  out->is_autofilled = data.is_autofilled();

  if (!data.ReadCheckStatus(&out->check_status))
    return false;

  out->is_focusable = data.is_focusable();
  out->should_autocomplete = data.should_autocomplete();

  if (!data.ReadRole(&out->role))
    return false;

  if (!data.ReadTextDirection(&out->text_direction))
    return false;

  out->is_enabled = data.is_enabled();
  out->is_readonly = data.is_readonly();
  if (!data.ReadTypedValue(&out->typed_value))
    return false;

  if (!data.ReadOptionValues(&out->option_values))
    return false;
  if (!data.ReadOptionContents(&out->option_contents))
    return false;

  if (!data.ReadLabelSource(&out->label_source))
    return false;

  if (!data.ReadBounds(&out->bounds))
    return false;

  if (!data.ReadDatalistValues(&out->datalist_values))
    return false;
  if (!data.ReadDatalistLabels(&out->datalist_labels))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::ButtonTitleInfoDataView,
                  autofill::ButtonTitleInfo>::
    Read(autofill::mojom::ButtonTitleInfoDataView data,
         autofill::ButtonTitleInfo* out) {
  return data.ReadTitle(&out->first) && data.ReadType(&out->second);
}

// static
bool StructTraits<autofill::mojom::FormDataDataView, autofill::FormData>::Read(
    autofill::mojom::FormDataDataView data,
    autofill::FormData* out) {
  if (!data.ReadIdAttribute(&out->id_attribute))
    return false;
  if (!data.ReadNameAttribute(&out->name_attribute))
    return false;
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadButtonTitles(&out->button_titles))
    return false;
  if (!data.ReadUrl(&out->url))
    return false;
  if (!data.ReadFullUrl(&out->full_url))
    return false;
  if (!data.ReadAction(&out->action))
    return false;
  out->is_action_empty = data.is_action_empty();
  if (!data.ReadMainFrameOrigin(&out->main_frame_origin))
    return false;

  out->is_form_tag = data.is_form_tag();
  out->is_formless_checkout = data.is_formless_checkout();

  if (!data.ReadUniqueRendererId(&out->unique_renderer_id))
    return false;

  if (!data.ReadSubmissionEvent(&out->submission_event))
    return false;

  if (!data.ReadFields(&out->fields))
    return false;

  if (!data.ReadUsernamePredictions(&out->username_predictions))
    return false;

  out->is_gaia_with_skip_save_password_form =
      data.is_gaia_with_skip_save_password_form();

  return true;
}

// static
bool StructTraits<autofill::mojom::FormFieldDataPredictionsDataView,
                  autofill::FormFieldDataPredictions>::
    Read(autofill::mojom::FormFieldDataPredictionsDataView data,
         autofill::FormFieldDataPredictions* out) {
  if (!data.ReadSignature(&out->signature))
    return false;
  if (!data.ReadHeuristicType(&out->heuristic_type))
    return false;
  if (!data.ReadServerType(&out->server_type))
    return false;
  if (!data.ReadOverallType(&out->overall_type))
    return false;
  if (!data.ReadParseableName(&out->parseable_name))
    return false;
  if (!data.ReadSection(&out->section))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::FormDataPredictionsDataView,
                  autofill::FormDataPredictions>::
    Read(autofill::mojom::FormDataPredictionsDataView data,
         autofill::FormDataPredictions* out) {
  if (!data.ReadData(&out->data))
    return false;
  if (!data.ReadSignature(&out->signature))
    return false;
  if (!data.ReadFields(&out->fields))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::PasswordAndMetadataDataView,
                  autofill::PasswordAndMetadata>::
    Read(autofill::mojom::PasswordAndMetadataDataView data,
         autofill::PasswordAndMetadata* out) {
  if (!data.ReadUsername(&out->username))
    return false;
  if (!data.ReadPassword(&out->password))
    return false;
  if (!data.ReadRealm(&out->realm))
    return false;

  out->uses_account_store = data.uses_account_store();

  return true;
}

// static
bool StructTraits<autofill::mojom::PasswordFormFillDataDataView,
                  autofill::PasswordFormFillData>::
    Read(autofill::mojom::PasswordFormFillDataDataView data,
         autofill::PasswordFormFillData* out) {
  if (!data.ReadFormRendererId(&out->form_renderer_id) ||
      !data.ReadUrl(&out->url) || !data.ReadAction(&out->action) ||
      !data.ReadUsernameField(&out->username_field) ||
      !data.ReadPasswordField(&out->password_field) ||
      !data.ReadPreferredRealm(&out->preferred_realm) ||
      !data.ReadAdditionalLogins(&out->additional_logins))
    return false;

  out->uses_account_store = data.uses_account_store();
  out->wait_for_username = data.wait_for_username();
  out->username_may_use_prefilled_placeholder =
      data.username_may_use_prefilled_placeholder();

  return true;
}

// static
bool StructTraits<autofill::mojom::PasswordFormGenerationDataDataView,
                  autofill::PasswordFormGenerationData>::
    Read(autofill::mojom::PasswordFormGenerationDataDataView data,
         autofill::PasswordFormGenerationData* out) {
  return data.ReadNewPasswordRendererId(&out->new_password_renderer_id) &&
         data.ReadConfirmationPasswordRendererId(
             &out->confirmation_password_renderer_id);
}

// static
bool StructTraits<autofill::mojom::PasswordGenerationUIDataDataView,
                  autofill::password_generation::PasswordGenerationUIData>::
    Read(autofill::mojom::PasswordGenerationUIDataDataView data,
         autofill::password_generation::PasswordGenerationUIData* out) {
  if (!data.ReadBounds(&out->bounds))
    return false;

  out->max_length = data.max_length();
  out->is_generation_element_password_type =
      data.is_generation_element_password_type();

  return data.ReadGenerationElementId(&out->generation_element_id) &&
         data.ReadGenerationElement(&out->generation_element) &&
         data.ReadTextDirection(&out->text_direction) &&
         data.ReadFormData(&out->form_data);
}

bool StructTraits<
    autofill::mojom::ParsingResultDataView,
    autofill::ParsingResult>::Read(autofill::mojom::ParsingResultDataView data,
                                   autofill::ParsingResult* out) {
  return data.ReadUsernameRendererId(&out->username_renderer_id) &&
         data.ReadPasswordRendererId(&out->password_renderer_id) &&
         data.ReadNewPasswordRendererId(&out->new_password_renderer_id) &&
         data.ReadConfirmPasswordRendererId(&out->confirm_password_renderer_id);
}

}  // namespace mojo

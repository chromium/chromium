// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_activity_params.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "ios/web/public/js_messaging/script_message.h"

using base::SysUTF8ToNSString;

namespace autofill {

BaseFormActivityParams::BaseFormActivityParams() = default;
BaseFormActivityParams::BaseFormActivityParams(
    const BaseFormActivityParams& other) = default;
BaseFormActivityParams::~BaseFormActivityParams() = default;

FormActivityParams::FormActivityParams() = default;
FormActivityParams::FormActivityParams(const FormActivityParams& other) =
    default;
FormActivityParams::~FormActivityParams() = default;

FormRemovalParams::FormRemovalParams() = default;
FormRemovalParams::FormRemovalParams(const FormRemovalParams& other) = default;
FormRemovalParams::~FormRemovalParams() = default;

bool BaseFormActivityParams::FromMessage(const web::ScriptMessage& message,
                                         const base::Value::Dict** message_body,
                                         BaseFormActivityParams* params) {
  if (!message.body() || !message.body()->is_dict()) {
    // Ignore invalid message.
    return false;
  }

  const auto& message_body_dict = message.body()->GetDict();
  *message_body = &message_body_dict;
  const std::string* frame_id = message_body_dict.FindString("frameID");
  if (!frame_id) {
    return false;
  }

  params->frame_id = *frame_id;
  params->is_main_frame = message.is_main_frame();

  return true;
}

bool BaseFormActivityParams::operator==(const BaseFormActivityParams&) const =
    default;

bool FormActivityParams::FromMessage(const web::ScriptMessage& message,
                                     FormActivityParams* params) {
  const base::Value::Dict* message_body = nullptr;
  if (!BaseFormActivityParams::FromMessage(message, &message_body, params)) {
    return false;
  }

  const std::string* form_name = message_body->FindString("formName");
  const std::string* form_renderer_id =
      message_body->FindString("formRendererID");
  const std::string* field_identifier =
      message_body->FindString("fieldIdentifier");
  const std::string* field_renderer_id =
      message_body->FindString("fieldRendererID");
  const std::string* field_type = message_body->FindString("fieldType");
  const std::string* type = message_body->FindString("type");
  const std::string* value = message_body->FindString("value");

  if (!form_name || !form_renderer_id) {
    params->input_missing = true;
  }

  if (form_name) {
    params->form_name = *form_name;
  }

  if (form_renderer_id) {
    // Parse the form renderer id.
    // Fallback to 0 if invalid or no form id provided, which is interpreted in
    // Autofill as the field not being owned by a form element.
    base::StringToUint(*form_renderer_id, &params->form_renderer_id.value());
  }

  std::optional<bool> has_user_gesture =
      message_body->FindBool("hasUserGesture");
  if (!field_identifier || !field_renderer_id || !field_type || !type ||
      !value || !has_user_gesture) {
    params->input_missing = true;
  }

  if (field_identifier) {
    params->field_identifier = *field_identifier;
  }
  if (field_renderer_id) {
    base::StringToUint(*field_renderer_id, &params->field_renderer_id.value());
  }
  if (field_type) {
    params->field_type = *field_type;
  }
  if (type) {
    params->type = *type;
  }
  if (value) {
    params->value = *value;
  }
  if (has_user_gesture) {
    params->has_user_gesture = *has_user_gesture;
  }

  return true;
}

bool FormActivityParams::operator==(const FormActivityParams& params) const {
  return BaseFormActivityParams::operator==(params) &&
         (form_name == params.form_name) &&
         (form_renderer_id == params.form_renderer_id) &&
         (field_identifier == params.field_identifier) &&
         (field_renderer_id == params.field_renderer_id) &&
         (field_type == params.field_type) && (value == params.value) &&
         (type == params.type) && (has_user_gesture == params.has_user_gesture);
}

bool FormRemovalParams::FromMessage(const web::ScriptMessage& message,
                                    FormRemovalParams* params) {
  const base::Value::Dict* message_body = nullptr;
  if (!BaseFormActivityParams::FromMessage(message, &message_body, params)) {
    return false;
  }

  // Parse array of form id's.
  if (const std::string* removed_form_ids =
          message_body->FindString("removedFormIDs")) {
    if (const auto extracted_form_ids =
            ExtractIDs<FormRendererId>(SysUTF8ToNSString(*removed_form_ids))) {
      params->removed_forms = std::move(*extracted_form_ids);
    }
  }

  // Parse array of field id's.
  if (const std::string* removed_field_ids =
          message_body->FindString("removedFieldIDs")) {
    if (const auto extracted_field_ids = ExtractIDs<FieldRendererId>(
            SysUTF8ToNSString(*removed_field_ids))) {
      params->removed_unowned_fields = *extracted_field_ids;
    }
  }

  // Params are valid if there are removed forms and/or removed unowned fields.
  return !params->removed_forms.empty() ||
         !params->removed_unowned_fields.empty();
}

}  // namespace autofill

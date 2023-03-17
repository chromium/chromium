// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/form_activity_params.h"

#include "base/strings/string_number_conversions.h"
#include "ios/web/public/js_messaging/script_message.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  const std::string* form_name = message_body_dict.FindString("formName");
  const std::string* unique_form_id =
      message_body_dict.FindString("uniqueFormID");
  if (!form_name || !unique_form_id) {
    params->input_missing = true;
  }

  if (form_name) {
    params->form_name = *form_name;
  }

  std::string unique_id;
  if (unique_form_id) {
    unique_id = *unique_form_id;
  }
  base::StringToUint(unique_id, &params->unique_form_id.value());

  params->is_main_frame = message.is_main_frame();

  return true;
}

bool FormActivityParams::FromMessage(const web::ScriptMessage& message,
                                     FormActivityParams* params) {
  const base::Value::Dict* message_body = nullptr;
  if (!BaseFormActivityParams::FromMessage(message, &message_body, params)) {
    return false;
  }

  const std::string* field_identifier =
      message_body->FindString("fieldIdentifier");
  const std::string* unique_field_id =
      message_body->FindString("uniqueFieldID");
  const std::string* field_type = message_body->FindString("fieldType");
  const std::string* type = message_body->FindString("type");
  const std::string* value = message_body->FindString("value");
  absl::optional<bool> has_user_gesture =
      message_body->FindBool("hasUserGesture");
  if (!field_identifier || !unique_field_id || !field_type || !type || !value ||
      !has_user_gesture) {
    params->input_missing = true;
  }

  if (field_identifier) {
    params->field_identifier = *field_identifier;
  }
  if (unique_field_id) {
    base::StringToUint(*unique_field_id, &params->unique_field_id.value());
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

}  // namespace autofill

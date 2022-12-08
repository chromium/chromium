// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/form_activity_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "components/autofill/ios/browser/autofill_util.h"
#include "components/autofill/ios/form_util/form_activity_observer.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "ios/web/public/js_messaging/script_message.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::NumberToString;
using base::StringToUint;
using base::SysUTF8ToNSString;

namespace autofill {

// static
FormActivityTabHelper* FormActivityTabHelper::GetOrCreateForWebState(
    web::WebState* web_state) {
  FormActivityTabHelper* helper = FromWebState(web_state);
  if (!helper) {
    CreateForWebState(web_state);
    helper = FromWebState(web_state);
    DCHECK(helper);
  }
  return helper;
}

FormActivityTabHelper::FormActivityTabHelper(web::WebState* web_state) {}
FormActivityTabHelper::~FormActivityTabHelper() = default;

void FormActivityTabHelper::AddObserver(FormActivityObserver* observer) {
  observers_.AddObserver(observer);
}

void FormActivityTabHelper::RemoveObserver(FormActivityObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FormActivityTabHelper::OnFormMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.body() || !message.body()->is_dict()) {
    // Ignore invalid message.
    return;
  }

  const std::string* command = message.body()->GetDict().FindString("command");
  if (!command) {
    DLOG(WARNING) << "JS message parameter not found: command";
  } else if (*command == "form.submit") {
    FormSubmissionHandler(web_state, message);
  } else if (*command == "form.activity") {
    HandleFormActivity(web_state, message);
  } else if (*command == "form.removal") {
    HandleFormRemoval(web_state, message);
  }
}

void FormActivityTabHelper::HandleFormActivity(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  const base::Value::Dict* message_body = nullptr;
  web::WebFrame* sender_frame = nullptr;
  FormActivityParams params;
  if (!GetBaseFormActivityParams(web_state, message, &message_body, &params,
                                 &sender_frame)) {
    return;
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
    params.input_missing = true;
  }

  if (field_identifier)
    params.field_identifier = *field_identifier;
  if (unique_field_id)
    StringToUint(*unique_field_id, &params.unique_field_id.value());
  if (field_type)
    params.field_type = *field_type;
  if (type)
    params.type = *type;
  if (value)
    params.value = *value;
  if (has_user_gesture)
    params.has_user_gesture = *has_user_gesture;

  for (auto& observer : observers_)
    observer.FormActivityRegistered(web_state, sender_frame, params);
}

void FormActivityTabHelper::HandleFormRemoval(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  const base::Value::Dict* message_body = nullptr;
  web::WebFrame* sender_frame = nullptr;
  FormRemovalParams params;
  if (!GetBaseFormActivityParams(web_state, message, &message_body, &params,
                                 &sender_frame)) {
    return;
  }

  if (!params.unique_form_id) {
    const std::string* unique_field_ids =
        message_body->FindString("uniqueFieldID");
    if (!unique_field_ids || !ExtractIDs(SysUTF8ToNSString(*unique_field_ids),
                                         &params.removed_unowned_fields)) {
      params.input_missing = true;
    }
  }

  for (auto& observer : observers_)
    observer.FormRemoved(web_state, sender_frame, params);
}

void FormActivityTabHelper::FormSubmissionHandler(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.body() || !message.body()->is_dict()) {
    // Ignore invalid message.
    return;
  }

  const base::Value::Dict& message_body = message.body()->GetDict();
  const std::string* frame_id = message_body.FindString("frameID");
  if (!frame_id) {
    return;
  }

  web::WebFrame* sender_frame = GetWebFrameWithId(web_state, *frame_id);
  if (!sender_frame) {
    return;
  }
  if (sender_frame->IsMainFrame() != message.is_main_frame()) {
    return;
  }

  if (!message_body.FindString("href")) {
    DLOG(WARNING) << "JS message parameter not found: href";
    return;
  }
  const std::string* maybe_form_name = message_body.FindString("formName");
  const std::string* maybe_form_data = message_body.FindString("formData");

  // We decide the form is user-submitted if the user has interacted with
  // the main page (using logic from the popup blocker), or if the keyboard
  // is visible.
  BOOL submitted_by_user = message.is_user_interacting() ||
                           [web_state->GetWebViewProxy() keyboardAccessory];

  std::string form_name;
  if (maybe_form_name) {
    form_name = *maybe_form_name;
  }
  std::string form_data;
  if (maybe_form_data) {
    form_data = *maybe_form_data;
  }
  for (auto& observer : observers_) {
    observer.DocumentSubmitted(web_state, sender_frame, form_name, form_data,
                               submitted_by_user);
  }
}

bool FormActivityTabHelper::GetBaseFormActivityParams(
    web::WebState* web_state,
    const web::ScriptMessage& message,
    const base::Value::Dict** message_body,
    BaseFormActivityParams* params,
    web::WebFrame** sender_frame) {
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

  *sender_frame = GetWebFrameWithId(web_state, *frame_id);
  if (!*sender_frame) {
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
  StringToUint(unique_id, &params->unique_form_id.value());

  params->is_main_frame = message.is_main_frame();

  return true;
}

WEB_STATE_USER_DATA_KEY_IMPL(FormActivityTabHelper)

}  // namespace autofill

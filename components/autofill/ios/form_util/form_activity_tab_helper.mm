// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/form_activity_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "components/autofill/ios/browser/autofill_util.h"
#include "components/autofill/ios/form_util/form_activity_observer.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#include "ios/web/public/js_messaging/script_message.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"

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
  } else if (*command == "pwdform.removal") {
    HandleFormRemoval(web_state, message);
  }
}

void FormActivityTabHelper::HandleFormActivity(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  FormActivityParams params;
  if (!FormActivityParams::FromMessage(message, &params)) {
    return;
  }

  web::WebFramesManager* frames_manager =
      FormUtilJavaScriptFeature::GetInstance()->GetWebFramesManager(web_state);
  web::WebFrame* sender_frame = frames_manager->GetFrameWithId(params.frame_id);
  if (!sender_frame) {
    return;
  }

  for (auto& observer : observers_)
    observer.FormActivityRegistered(web_state, sender_frame, params);
}

void FormActivityTabHelper::HandleFormRemoval(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  const base::Value::Dict* message_body = nullptr;
  FormRemovalParams params;
  if (!BaseFormActivityParams::FromMessage(message, &message_body, &params)) {
    return;
  }

  web::WebFramesManager* frames_manager =
      FormUtilJavaScriptFeature::GetInstance()->GetWebFramesManager(web_state);
  web::WebFrame* sender_frame = frames_manager->GetFrameWithId(params.frame_id);
  if (!sender_frame) {
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

  web::WebFramesManager* frames_manager =
      FormUtilJavaScriptFeature::GetInstance()->GetWebFramesManager(web_state);
  web::WebFrame* sender_frame = frames_manager->GetFrameWithId(*frame_id);
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
                           web_state->GetWebViewProxy().keyboardVisible;

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

WEB_STATE_USER_DATA_KEY_IMPL(FormActivityTabHelper)

}  // namespace autofill

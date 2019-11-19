// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/form_activity_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/values.h"
#include "components/autofill/ios/form_util/form_activity_observer.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

namespace {
// Prefix for the form activity event commands. Must be kept in sync with
// form.js.
const char kCommandPrefix[] = "form";
}

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

FormActivityTabHelper::FormActivityTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
  subscription_ = web_state_->AddScriptCommandCallback(
      base::BindRepeating(&FormActivityTabHelper::OnFormCommand,
                          base::Unretained(this)),
      kCommandPrefix);
}

FormActivityTabHelper::~FormActivityTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void FormActivityTabHelper::AddObserver(FormActivityObserver* observer) {
  observers_.AddObserver(observer);
}

void FormActivityTabHelper::RemoveObserver(FormActivityObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FormActivityTabHelper::OnFormCommand(const base::DictionaryValue& message,
                                          const GURL& url,
                                          bool user_is_interacting,
                                          web::WebFrame* sender_frame) {
  std::string command;
  if (!message.GetString("command", &command)) {
    DLOG(WARNING) << "JS message parameter not found: command";
  } else if (command == "form.submit") {
    FormSubmissionHandler(message, user_is_interacting,
                          sender_frame->IsMainFrame(), sender_frame);
  } else if (command == "form.activity") {
    HandleFormActivity(message, user_is_interacting,
                       sender_frame->IsMainFrame(), sender_frame);
  }
}

bool FormActivityTabHelper::HandleFormActivity(
    const base::DictionaryValue& message,
    bool has_user_gesture,
    bool form_in_main_frame,
    web::WebFrame* sender_frame) {
  FormActivityParams params;
  if (!message.GetString("formName", &params.form_name) ||
      !message.GetString("fieldIdentifier", &params.field_identifier) ||
      !message.GetString("fieldType", &params.field_type) ||
      !message.GetString("type", &params.type) ||
      !message.GetString("value", &params.value) ||
      !message.GetBoolean("hasUserGesture", &params.has_user_gesture)) {
    params.input_missing = true;
  }

  params.is_main_frame = form_in_main_frame;
  if (!sender_frame) {
    return false;
  }
  params.frame_id = sender_frame->GetFrameId();
  for (auto& observer : observers_)
    observer.FormActivityRegistered(web_state_, sender_frame, params);
  return true;
}

bool FormActivityTabHelper::FormSubmissionHandler(
    const base::DictionaryValue& message,
    bool has_user_gesture,
    bool form_in_main_frame,
    web::WebFrame* sender_frame) {
  std::string href;
  if (!message.GetString("href", &href)) {
    DLOG(WARNING) << "JS message parameter not found: href";
    return false;
  }
  std::string form_name;
  message.GetString("formName", &form_name);

  std::string form_data;
  message.GetString("formData", &form_data);
  // We decide the form is user-submitted if the user has interacted with
  // the main page (using logic from the popup blocker), or if the keyboard
  // is visible.
  BOOL submitted_by_user =
      has_user_gesture || [web_state_->GetWebViewProxy() keyboardAccessory];

  for (auto& observer : observers_)
    observer.DocumentSubmitted(web_state_, sender_frame, form_name, form_data,
                               submitted_by_user, form_in_main_frame);
  return true;
}

void FormActivityTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(FormActivityTabHelper)

}  // namespace autofill

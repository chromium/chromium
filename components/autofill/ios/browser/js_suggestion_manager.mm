// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/js_suggestion_manager.h"

#import <Foundation/Foundation.h>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/format_macros.h"
#include "base/json/string_escape.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "components/autofill/ios/browser/autofill_util.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

JsSuggestionManager::JsSuggestionManager(web::WebState* web_state)
    : web_state_(web_state), weak_ptr_factory_(this) {}

JsSuggestionManager::~JsSuggestionManager() = default;

// static
JsSuggestionManager* JsSuggestionManager::GetOrCreateForWebState(
    web::WebState* web_state) {
  JsSuggestionManager* helper = FromWebState(web_state);
  if (!helper) {
    CreateForWebState(web_state);
    helper = FromWebState(web_state);
    DCHECK(helper);
  }
  return helper;
}

void JsSuggestionManager::SelectNextElementInFrameWithID(
    const std::string& frame_ID) {
  SelectNextElementInFrameWithID(frame_ID, "", "");
}

void JsSuggestionManager::SelectNextElementInFrameWithID(
    const std::string& frame_ID,
    const std::string& form_name,
    const std::string& field_name) {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(form_name));
  parameters.push_back(base::Value(field_name));
  autofill::ExecuteJavaScriptFunction("suggestion.selectNextElement",
                                      parameters, GetFrameWithFrameID(frame_ID),
                                      autofill::JavaScriptResultCallback());
}

void JsSuggestionManager::SelectPreviousElementInFrameWithID(
    const std::string& frame_ID) {
  SelectPreviousElementInFrameWithID(frame_ID, "", "");
}

void JsSuggestionManager::SelectPreviousElementInFrameWithID(
    const std::string& frame_ID,
    const std::string& form_name,
    const std::string& field_name) {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(form_name));
  parameters.push_back(base::Value(field_name));
  autofill::ExecuteJavaScriptFunction("suggestion.selectPreviousElement",
                                      parameters, GetFrameWithFrameID(frame_ID),
                                      autofill::JavaScriptResultCallback());
}

void JsSuggestionManager::FetchPreviousAndNextElementsPresenceInFrameWithID(
    const std::string& frame_ID,
    base::OnceCallback<void(bool, bool)> completion_handler) {
  FetchPreviousAndNextElementsPresenceInFrameWithID(
      frame_ID, "", "", std::move(completion_handler));
}

void JsSuggestionManager::FetchPreviousAndNextElementsPresenceInFrameWithID(
    const std::string& frame_ID,
    const std::string& form_name,
    const std::string& field_name,
    base::OnceCallback<void(bool, bool)> completion_handler) {
  DCHECK(completion_handler);
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(form_name));
  parameters.push_back(base::Value(field_name));
  autofill::ExecuteJavaScriptFunction(
      "suggestion.hasPreviousNextElements", parameters,
      GetFrameWithFrameID(frame_ID),
      base::BindOnce(
          &JsSuggestionManager::PreviousAndNextElementsPresenceResult,
          weak_ptr_factory_.GetWeakPtr(), std::move(completion_handler)));
}

void JsSuggestionManager::PreviousAndNextElementsPresenceResult(
    base::OnceCallback<void(bool, bool)> completion_handler,
    const base::Value* res) {
  NSString* result = nil;
  if (res && res->is_string()) {
    result = base::SysUTF8ToNSString(res->GetString());
  }
  // The result maybe an empty string here due to 2 reasons:
  // 1) When there is an exception running the JS
  // 2) There is a race when the page is changing due to which
  // JSSuggestionManager has not yet injected __gCrWeb.suggestion
  // object Handle this case gracefully. If a page has overridden
  // Array.toString, the string returned may not contain a ",",
  // hence this is a defensive measure to early return.
  NSArray* components = [result componentsSeparatedByString:@","];
  if (components.count != 2) {
    std::move(completion_handler).Run(false, false);
    return;
  }

  DCHECK([components[0] isEqualToString:@"true"] ||
         [components[0] isEqualToString:@"false"]);
  bool has_previous_element = [components[0] isEqualToString:@"true"];
  DCHECK([components[1] isEqualToString:@"true"] ||
         [components[1] isEqualToString:@"false"]);
  bool has_next_element = [components[1] isEqualToString:@"true"];
  std::move(completion_handler).Run(has_previous_element, has_next_element);
}

void JsSuggestionManager::CloseKeyboardForFrameWithID(
    const std::string& frame_ID) {
  std::vector<base::Value> parameters;
  autofill::ExecuteJavaScriptFunction("suggestion.blurActiveElement",
                                      parameters, GetFrameWithFrameID(frame_ID),
                                      autofill::JavaScriptResultCallback());
}

web::WebFrame* JsSuggestionManager::GetFrameWithFrameID(
    const std::string& frame_ID) {
  return web_state_->GetWebFramesManager()->GetFrameWithId(frame_ID);
}

WEB_STATE_USER_DATA_KEY_IMPL(JsSuggestionManager)

}  // namspace autofill

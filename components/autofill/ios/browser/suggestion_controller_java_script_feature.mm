// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/suggestion_controller_java_script_feature.h"

#import <Foundation/Foundation.h>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

namespace {

const char kScriptName[] = "suggestion_controller";

// The timeout for any JavaScript call in this file.
const int64_t kJavaScriptExecutionTimeoutInSeconds = 5;

void ProcessPreviousAndNextElementsPresenceResult(
    base::OnceCallback<void(bool, bool)> completion_handler,
    const base::Value* res) {
  NSString* result = nil;
  if (res && res->is_string()) {
    result = base::SysUTF8ToNSString(res->GetString());
  }
  // The result maybe an empty string here due to 2 reasons:
  // 1) When there is an exception running the JS
  // 2) There is a race when the page is changing due to which
  // SuggestionControllerJavaScriptFeature has not yet injected the
  // __gCrWeb.suggestion object.
  // Handle this case gracefully. If a page has overridden
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

}  // namespace

// static
SuggestionControllerJavaScriptFeature*
SuggestionControllerJavaScriptFeature::GetInstance() {
  static base::NoDestructor<SuggestionControllerJavaScriptFeature> instance;
  return instance.get();
}

SuggestionControllerJavaScriptFeature::SuggestionControllerJavaScriptFeature()
    : web::JavaScriptFeature(
          // TODO(crbug.com/1175793): Move autofill code to kIsolatedWorld
          // once all scripts are converted to JavaScriptFeatures.
          web::ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {AutofillJavaScriptFeature::GetInstance()}) {}

SuggestionControllerJavaScriptFeature::
    ~SuggestionControllerJavaScriptFeature() = default;

void SuggestionControllerJavaScriptFeature::SelectNextElementInFrame(
    web::WebFrame* frame) {
  SelectNextElementInFrame(frame, "", "");
}

void SuggestionControllerJavaScriptFeature::SelectNextElementInFrame(
    web::WebFrame* frame,
    const std::string& form_name,
    const std::string& field_name) {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(form_name));
  parameters.push_back(base::Value(field_name));
  CallJavaScriptFunction(frame, "suggestion.selectNextElement", parameters);
}

void SuggestionControllerJavaScriptFeature::SelectPreviousElementInFrame(
    web::WebFrame* frame) {
  SelectPreviousElementInFrame(frame, "", "");
}

void SuggestionControllerJavaScriptFeature::SelectPreviousElementInFrame(
    web::WebFrame* frame,
    const std::string& form_name,
    const std::string& field_name) {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(form_name));
  parameters.push_back(base::Value(field_name));
  CallJavaScriptFunction(frame, "suggestion.selectPreviousElement", parameters);
}

void SuggestionControllerJavaScriptFeature::
    FetchPreviousAndNextElementsPresenceInFrame(
        web::WebFrame* frame,
        base::OnceCallback<void(bool, bool)> completion_handler) {
  FetchPreviousAndNextElementsPresenceInFrame(frame, "", "",
                                              std::move(completion_handler));
}

void SuggestionControllerJavaScriptFeature::
    FetchPreviousAndNextElementsPresenceInFrame(
        web::WebFrame* frame,
        const std::string& form_name,
        const std::string& field_name,
        base::OnceCallback<void(bool, bool)> completion_handler) {
  DCHECK(completion_handler);
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(form_name));
  parameters.push_back(base::Value(field_name));
  CallJavaScriptFunction(
      frame, "suggestion.hasPreviousNextElements", parameters,
      base::BindOnce(&ProcessPreviousAndNextElementsPresenceResult,
                     std::move(completion_handler)),
      base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

}  // namespace autofill

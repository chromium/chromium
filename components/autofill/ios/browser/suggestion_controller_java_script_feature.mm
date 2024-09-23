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
#import "components/autofill/ios/common/javascript_feature_util.h"

namespace autofill {

namespace {

const char kScriptName[] = "suggestion_controller";

// The timeout for any JavaScript call in this file.
const int64_t kJavaScriptExecutionTimeoutInSeconds = 5;

void ProcessPreviousAndNextElementsPresenceResult(
    base::OnceCallback<void(bool, bool)> completion_handler,
    const base::Value* res) {
  // The result may be invalid:
  // 1) When there is an exception running the JS
  // 2) There is a race when the page is changing due to which
  // SuggestionControllerJavaScriptFeature has not yet injected the
  // __gCrWeb.suggestion object.
  // Handle this case gracefully.
  if (!res || !res->is_dict() || res->GetDict().size() != 2) {
    std::move(completion_handler).Run(false, false);
    return;
  }

  const base::Value::Dict& dict = res->GetDict();
  std::optional<bool> previous = dict.FindBool("previous");
  std::optional<bool> next = dict.FindBool("next");
  if (!previous || !next) {
    std::move(completion_handler).Run(false, false);
    return;
  }

  std::move(completion_handler).Run(previous.value(), next.value());
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
          ContentWorldForAutofillJavascriptFeatures(),
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
  CallJavaScriptFunction(
      frame, "suggestion.selectNextElement",
      base::Value::List().Append(form_name).Append(field_name));
}

void SuggestionControllerJavaScriptFeature::SelectPreviousElementInFrame(
    web::WebFrame* frame) {
  SelectPreviousElementInFrame(frame, "", "");
}

void SuggestionControllerJavaScriptFeature::SelectPreviousElementInFrame(
    web::WebFrame* frame,
    const std::string& form_name,
    const std::string& field_name) {
  CallJavaScriptFunction(
      frame, "suggestion.selectPreviousElement",
      base::Value::List().Append(form_name).Append(field_name));
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
  CallJavaScriptFunction(
      frame, "suggestion.hasPreviousNextElements",
      base::Value::List().Append(form_name).Append(field_name),
      base::BindOnce(&ProcessPreviousAndNextElementsPresenceResult,
                     std::move(completion_handler)),
      base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

}  // namespace autofill

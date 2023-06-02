// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_java_script_feature.h"

#import <Foundation/Foundation.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "autofill_controller";

// The timeout for any JavaScript call in this file.
const int64_t kJavaScriptExecutionTimeoutInSeconds = 5;

}  // namespace

namespace autofill {

// static
AutofillJavaScriptFeature* AutofillJavaScriptFeature::GetInstance() {
  static base::NoDestructor<AutofillJavaScriptFeature> instance;
  return instance.get();
}

AutofillJavaScriptFeature::AutofillJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {FormUtilJavaScriptFeature::GetInstance()}) {}

AutofillJavaScriptFeature::~AutofillJavaScriptFeature() = default;

void AutofillJavaScriptFeature::AddJSDelayInFrame(web::WebFrame* frame) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          autofill::switches::kAutofillIOSDelayBetweenFields)) {
    return;
  }

  const std::string delay_string = command_line->GetSwitchValueASCII(
      autofill::switches::kAutofillIOSDelayBetweenFields);
  int command_line_delay = 0;
  if (base::StringToInt(delay_string, &command_line_delay)) {
    return;
  }

  CallJavaScriptFunction(frame, "autofill.setDelay",
                         base::Value::List().Append(command_line_delay));
}

void AutofillJavaScriptFeature::FetchForms(
    web::WebFrame* frame,
    NSUInteger required_fields_count,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(!callback.is_null());

  bool restrict_unowned_fields_to_formless_checkout = false;
  CallJavaScriptFunction(
      frame, "autofill.extractForms",
      base::Value::List()
          .Append(static_cast<int>(required_fields_count))
          .Append(restrict_unowned_fields_to_formless_checkout),
      autofill::CreateStringCallback(std::move(callback)),
      base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void AutofillJavaScriptFeature::FillActiveFormField(
    web::WebFrame* frame,
    base::Value::Dict data,
    base::OnceCallback<void(BOOL)> callback) {
  CallJavaScriptFunction(frame, "autofill.fillActiveFormField",
                         base::Value::List().Append(std::move(data)),
                         autofill::CreateBoolCallback(std::move(callback)),
                         base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void AutofillJavaScriptFeature::FillForm(
    web::WebFrame* frame,
    base::Value::Dict data,
    autofill::FieldRendererId force_fill_field_id,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(!callback.is_null());

  CallJavaScriptFunction(
      frame, "autofill.fillForm",
      base::Value::List()
          .Append(std::move(data))
          .Append(static_cast<int>(force_fill_field_id.value())),
      autofill::CreateStringCallback(std::move(callback)),
      base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void AutofillJavaScriptFeature::ClearAutofilledFieldsForForm(
    web::WebFrame* frame,
    autofill::FormRendererId form_renderer_id,
    autofill::FieldRendererId field_renderer_id,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(!callback.is_null());

  CallJavaScriptFunction(
      frame, "autofill.clearAutofilledFields",
      base::Value::List()
          .Append(static_cast<int>(form_renderer_id.value()))
          .Append(static_cast<int>(field_renderer_id.value())),
      autofill::CreateStringCallback(std::move(callback)),
      base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void AutofillJavaScriptFeature::FillPredictionData(web::WebFrame* frame,
                                                   base::Value::Dict data) {
  CallJavaScriptFunction(frame, "autofill.fillPredictionData",
                         base::Value::List().Append(std::move(data)));
}

}  // namespace autofill

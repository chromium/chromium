// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_java_script_feature.h"

#import <Foundation/Foundation.h>

#include "base/command_line.h"
#include "base/feature_list.h"
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
const char kScriptName[] = "autofill_js";

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
          // TODO(crbug.com/1175793): Move autofill code to kAnyContentWorld
          // once all scripts are converted to JavaScriptFeatures.
          ContentWorld::kPageContentWorld,
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

  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(command_line_delay));
  CallJavaScriptFunction(frame, "autofill.setDelay", parameters);
}

void AutofillJavaScriptFeature::FetchForms(
    web::WebFrame* frame,
    NSUInteger required_fields_count,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(!callback.is_null());

  bool restrict_unowned_fields_to_formless_checkout = false;
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(static_cast<int>(required_fields_count)));
  parameters.push_back(
      base::Value(restrict_unowned_fields_to_formless_checkout));
  CallJavaScriptFunction(frame, "autofill.extractForms", parameters,
                         autofill::CreateStringCallback(std::move(callback)),
                         base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void AutofillJavaScriptFeature::FillActiveFormField(
    web::WebFrame* frame,
    std::unique_ptr<base::DictionaryValue> data,
    base::OnceCallback<void(BOOL)> callback) {
  DCHECK(data);

  bool has_render_id = !!data->FindKey("unique_renderer_id");
  bool use_renderer_ids =
      has_render_id &&
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillUseUniqueRendererIDsOnIOS);
  const std::string filling_function =
      use_renderer_ids ? "autofill.fillActiveFormFieldUsingRendererIDs"
                       : "autofill.fillActiveFormField";

  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  CallJavaScriptFunction(frame, filling_function, parameters,
                         autofill::CreateBoolCallback(std::move(callback)),
                         base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void AutofillJavaScriptFeature::FillForm(
    web::WebFrame* frame,
    std::unique_ptr<base::Value> data,
    NSString* force_fill_field_identifier,
    autofill::FieldRendererId force_fill_field_unique_id,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(data);
  DCHECK(!callback.is_null());

  bool use_renderer_ids = base::FeatureList::IsEnabled(
      autofill::features::kAutofillUseUniqueRendererIDsOnIOS);

  const std::string field_string_id =
      force_fill_field_identifier
          ? base::SysNSStringToUTF8(force_fill_field_identifier)
          : "null";
  int field_numeric_id = force_fill_field_unique_id
                             ? force_fill_field_unique_id.value()
                             : FieldRendererId().value();
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  parameters.push_back(base::Value(field_string_id));
  parameters.push_back(base::Value(field_numeric_id));
  parameters.push_back(base::Value(use_renderer_ids));
  CallJavaScriptFunction(frame, "autofill.fillForm", parameters,
                         autofill::CreateStringCallback(std::move(callback)),
                         base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void AutofillJavaScriptFeature::ClearAutofilledFieldsForFormName(
    web::WebFrame* frame,
    NSString* form_name,
    autofill::FormRendererId form_renderer_id,
    NSString* field_identifier,
    autofill::FieldRendererId field_renderer_id,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(!callback.is_null());

  bool use_renderer_ids = base::FeatureList::IsEnabled(
      autofill::features::kAutofillUseUniqueRendererIDsOnIOS);
  int form_numeric_id =
      form_renderer_id ? form_renderer_id.value() : FieldRendererId().value();
  int field_numeric_id =
      field_renderer_id ? field_renderer_id.value() : FieldRendererId().value();

  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(base::SysNSStringToUTF8(form_name)));
  parameters.push_back(base::Value(form_numeric_id));
  parameters.push_back(base::Value(base::SysNSStringToUTF8(field_identifier)));
  parameters.push_back(base::Value(field_numeric_id));
  parameters.push_back(base::Value(use_renderer_ids));
  CallJavaScriptFunction(frame, "autofill.clearAutofilledFields", parameters,
                         autofill::CreateStringCallback(std::move(callback)),
                         base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void AutofillJavaScriptFeature::FillPredictionData(
    web::WebFrame* frame,
    std::unique_ptr<base::Value> data) {
  DCHECK(data);
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  CallJavaScriptFunction(frame, "autofill.fillPredictionData", parameters);
}

}  // namespace autofill

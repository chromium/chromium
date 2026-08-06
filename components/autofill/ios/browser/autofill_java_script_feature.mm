// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_java_script_feature.h"

#import <Foundation/Foundation.h>

#import "base/command_line.h"
#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/common/autofill_optimization_features.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {
const char kScriptName[] = "autofill_controller";
constexpr char kFormFilledCommand[] = "formFilled";

// The timeout for any JavaScript call in this file.
constexpr base::TimeDelta kJavaScriptExecutionTimeout = base::Seconds(5);

}  // namespace

namespace autofill {

// static
AutofillJavaScriptFeature* AutofillJavaScriptFeature::GetInstance() {
  static base::NoDestructor<AutofillJavaScriptFeature> instance;
  return instance.get();
}

AutofillJavaScriptFeature::AutofillJavaScriptFeature()
    : web::JavaScriptFeature(
          ContentWorldForAutofillJavascriptFeatures(),
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow,
              base::BindRepeating(
                  []() -> FeatureScript::PlaceholderReplacements {
                    return @{
                      @"window."
                      @"gCrWebPlaceholderAutofillOptimizationFormSearch" :
                              base::FeatureList::IsEnabled(
                                  features::kAutofillOptimizationFormSearchIos)
                          ? @"true"
                          : @"false",
                    };
                  }))},
          {AutofillFormFeaturesJavaScriptFeature::GetInstance()}) {}

AutofillJavaScriptFeature::~AutofillJavaScriptFeature() = default;

void AutofillJavaScriptFeature::FetchForms(
    web::WebFrame* frame,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(!callback.is_null());

  bool restrict_unowned_fields_to_formless_checkout = false;
  CallJavaScriptFunction(
      frame, "autofill.extractForms",
      base::ListValue().Append(restrict_unowned_fields_to_formless_checkout),
      CreateStringCallback(std::move(callback)), kJavaScriptExecutionTimeout);
}

void AutofillJavaScriptFeature::FillActiveFormField(
    web::WebFrame* frame,
    base::DictValue data,
    base::OnceCallback<void(BOOL)> callback) {
  CallJavaScriptFunction(frame, "autofill.fillActiveFormField",
                         base::ListValue().Append(std::move(data)),
                         CreateBoolCallback(std::move(callback)),
                         kJavaScriptExecutionTimeout);
}

void AutofillJavaScriptFeature::FillSpecificFormField(
    web::WebFrame* frame,
    base::DictValue data,
    base::OnceCallback<void(BOOL)> callback) {
  CallJavaScriptFunction(frame, "autofill.fillSpecificFormField",
                         base::ListValue().Append(std::move(data)),
                         CreateBoolCallback(std::move(callback)),
                         kJavaScriptExecutionTimeout);
}

void AutofillJavaScriptFeature::FillForm(
    web::WebFrame* frame,
    base::DictValue data,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(!callback.is_null());

  CallJavaScriptFunction(
      frame, "autofill.fillForm", base::ListValue().Append(std::move(data)),
      CreateStringCallback(std::move(callback)), kJavaScriptExecutionTimeout);
}

void AutofillJavaScriptFeature::FillPredictionData(web::WebFrame* frame,
                                                   base::DictValue data) {
  CallJavaScriptFunction(frame, "autofill.fillPredictionData",
                         base::ListValue().Append(std::move(data)));
}

std::optional<std::string>
AutofillJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptName;
}

void AutofillJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.legacy_body() || !message.legacy_body()->is_dict()) {
    return;
  }
  const std::string* command =
      message.legacy_body()->GetDict().FindString("command");
  const std::string* frame_id =
      message.legacy_body()->GetDict().FindString("frame");
  const base::DictValue* form_dict =
      message.legacy_body()->GetDict().FindDict("form_data");
  if (!command || !frame_id || !form_dict || *command != kFormFilledCommand) {
    return;
  }

  web::WebFrame* frame =
      GetWebFramesManager(web_state)->GetFrameWithId(*frame_id);
  if (!frame) {
    return;
  }

  auto* driver = AutofillDriverIOS::FromWebStateAndWebFrame(web_state, frame);

  const scoped_refptr<FieldDataManager> field_data_manager =
      FieldDataManagerFactoryIOS::GetRetainable(frame);

  if (base::expected<FormData, ExtractFormDataFailure> form_data =
          ExtractFormData(*form_dict, /*form_name_filter=*/std::nullopt,
                          web_state->GetLastCommittedURL(),
                          frame->GetSecurityOrigin(), frame->GetUrl(),
                          *field_data_manager, frame->GetFrameId());
      form_data.has_value()) {
    driver->DidAutofillForm(std::move(form_data).value());
  }
}

}  // namespace autofill

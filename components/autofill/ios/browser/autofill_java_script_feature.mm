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
#import "base/values.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"
#import "components/autofill/ios/form_util/autofill_renderer_id_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {
const char kScriptName[] = "autofill_controller";
constexpr char kFormFilledCommand[] = "formFilled";

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
          ContentWorldForAutofillJavascriptFeatures(),
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {FormUtilJavaScriptFeature::GetInstance(),
           AutofillFormFeaturesJavaScriptFeature::GetInstance(),
           AutofillRendererIDJavaScriptFeature::GetInstance()}) {}

AutofillJavaScriptFeature::~AutofillJavaScriptFeature() = default;

void AutofillJavaScriptFeature::FetchForms(
    web::WebFrame* frame,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(!callback.is_null());

  bool restrict_unowned_fields_to_formless_checkout = false;
  CallJavaScriptFunction(
      frame, "autofill.extractForms",
      base::Value::List().Append(restrict_unowned_fields_to_formless_checkout),
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

void AutofillJavaScriptFeature::FillSpecificFormField(
    web::WebFrame* frame,
    base::Value::Dict data,
    base::OnceCallback<void(BOOL)> callback) {
  CallJavaScriptFunction(frame, "autofill.fillSpecificFormField",
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

std::optional<std::string>
AutofillJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptName;
}

void AutofillJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.body() || !message.body()->is_dict()) {
    return;
  }
  const std::string* command = message.body()->GetDict().FindString("command");
  const std::string* frame_id = message.body()->GetDict().FindString("frame");
  const base::Value::Dict* form_dict =
      message.body()->GetDict().FindDict("form_data");
  if (!command || !frame_id || !form_dict || *command != kFormFilledCommand) {
    return;
  }

  web::WebFrame* frame =
      GetWebFramesManager(web_state)->GetFrameWithId(*frame_id);
  if (!frame) {
    return;
  }

  auto* driver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(web_state, frame);

  const scoped_refptr<FieldDataManager> field_data_manager =
      FieldDataManagerFactoryIOS::GetRetainable(frame);

  autofill::FormData form_data;
  if (!ExtractFormData(*form_dict, /*filtered=*/false, /*form_name=*/u"",
                       web_state->GetLastCommittedURL(),
                       frame->GetSecurityOrigin(), *field_data_manager,
                       frame->GetFrameId(), &form_data)) {
    return;
  }

  driver->DidFillAutofillFormData(form_data, base::TimeTicks::Now());
}

}  // namespace autofill

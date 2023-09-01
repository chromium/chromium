// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"

#include "base/no_destructor.h"
#include "base/values.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#include "components/autofill/ios/form_util/form_activity_tab_helper.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#include "ios/web/public/js_messaging/java_script_feature_util.h"

namespace {
constexpr char kScriptName[] = "form_handlers";
constexpr char kScriptMessageName[] = "FormHandlersMessage";
}  // namespace

namespace autofill {

// static
FormHandlersJavaScriptFeature* FormHandlersJavaScriptFeature::GetInstance() {
  static base::NoDestructor<FormHandlersJavaScriptFeature> instance;
  return instance.get();
}

FormHandlersJavaScriptFeature::FormHandlersJavaScriptFeature()
    : web::JavaScriptFeature(
          ContentWorldForAutofillJavascriptFeatures(),
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::
                  kReinjectOnDocumentRecreation)},
          {web::java_script_features::GetCommonJavaScriptFeature(),
           autofill::FormUtilJavaScriptFeature::GetInstance(),
           password_manager::PasswordManagerJavaScriptFeature::GetInstance()}) {
}

FormHandlersJavaScriptFeature::~FormHandlersJavaScriptFeature() = default;

void FormHandlersJavaScriptFeature::TrackFormMutations(
    web::WebFrame* frame,
    int mutation_tracking_delay) {
  CallJavaScriptFunction(frame, "formHandlers.trackFormMutations",
                         base::Value::List().Append(mutation_tracking_delay));
}

void FormHandlersJavaScriptFeature::ToggleTrackingUserEditedFields(
    web::WebFrame* frame,
    bool track_user_edited_fields) {
  CallJavaScriptFunction(frame, "formHandlers.toggleTrackingUserEditedFields",
                         base::Value::List().Append(track_user_edited_fields));
}

absl::optional<std::string>
FormHandlersJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptMessageName;
}

void FormHandlersJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  FormActivityTabHelper* helper =
      FormActivityTabHelper::GetOrCreateForWebState(web_state);
  helper->OnFormMessageReceived(web_state, message);
}

}  // namespace autofill

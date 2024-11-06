// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"

#import "base/no_destructor.h"
#import "base/values.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"
#import "components/autofill/ios/form_util/autofill_renderer_id_java_script_feature.h"
#import "components/autofill/ios/form_util/form_activity_tab_helper.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/autofill/ios/form_util/remote_frame_registration_java_script_feature.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {

using FeatureScript = web::JavaScriptFeature::FeatureScript;

constexpr char kFormHandlerScriptName[] = "form_handlers";
constexpr char kRemoteTokenRegistrationScriptName[] =
    "register_remote_frame_token";
constexpr char kScriptMessageName[] = "FormHandlersMessage";

std::vector<web::JavaScriptFeature::FeatureScript> GetFeatureScripts() {
  std::vector<FeatureScript> feature_scripts;

  feature_scripts.push_back(FeatureScript::CreateWithFilename(
      kFormHandlerScriptName, FeatureScript::InjectionTime::kDocumentStart,
      FeatureScript::TargetFrames::kAllFrames,
      FeatureScript::ReinjectionBehavior::kReinjectOnDocumentRecreation));

  if (base::FeatureList::IsEnabled(kAutofillIsolatedWorldForJavascriptIos)) {
    feature_scripts.push_back(FeatureScript::CreateWithFilename(
        kRemoteTokenRegistrationScriptName,
        FeatureScript::InjectionTime::kDocumentStart,
        FeatureScript::TargetFrames::kAllFrames,
        FeatureScript::ReinjectionBehavior::kReinjectOnDocumentRecreation));
  }

  return feature_scripts;
}

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
          GetFeatureScripts(),
          {
              web::java_script_features::GetCommonJavaScriptFeature(),
              autofill::AutofillFormFeaturesJavaScriptFeature::GetInstance(),
              autofill::FormUtilJavaScriptFeature::GetInstance(),
              AutofillRendererIDJavaScriptFeature::GetInstance(),
              RemoteFrameRegistrationJavaScriptFeature::GetInstance(),
          }) {}

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

std::optional<std::string>
FormHandlersJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptMessageName;
}

void FormHandlersJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  // Delegate to FormActivityTabHelper for all other messages.
  FormActivityTabHelper* helper =
      FormActivityTabHelper::GetOrCreateForWebState(web_state);
  helper->OnFormMessageReceived(web_state, message);
}

FormHandlersJavaScriptFeature::FormHandlersJavaScriptFeature(
    AutofillRendererIDJavaScriptFeature* renderer_id_feature,
    RemoteFrameRegistrationJavaScriptFeature*
        remote_frame_registration_java_script_feature)
    : web::JavaScriptFeature(
          ContentWorldForAutofillJavascriptFeatures(),
          GetFeatureScripts(),
          {
              web::java_script_features::GetCommonJavaScriptFeature(),
              FormUtilJavaScriptFeature::GetInstance(),
              renderer_id_feature,
              remote_frame_registration_java_script_feature,
          }) {}

}  // namespace autofill

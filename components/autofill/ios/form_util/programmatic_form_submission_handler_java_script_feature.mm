// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/programmatic_form_submission_handler_java_script_feature.h"

#import <optional>

#import "base/feature_list.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"
#import "components/autofill/ios/form_util/form_activity_tab_helper.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {
constexpr char kScriptName[] = "programmatic_form_submission_handler";
constexpr char kScriptMessageName[] =
    "ProgrammaticFormSubmissionHandlerMessage";
}  // namespace

namespace autofill {

// static
ProgrammaticFormSubmissionHandlerJavaScriptFeature*
ProgrammaticFormSubmissionHandlerJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ProgrammaticFormSubmissionHandlerJavaScriptFeature>
      instance;
  return instance.get();
}

ProgrammaticFormSubmissionHandlerJavaScriptFeature::
    ProgrammaticFormSubmissionHandlerJavaScriptFeature()
    : web::JavaScriptFeature(
          // This feature modifies the prototype of HTMLFormElement. The script
          // has to be injected in the page content world so the page scripts
          // use the modified JavaScript prototype. JavaScript variables are not
          // shared across content worlds, which include prototypes. That is why
          // the script would have no effect if injected in an isolated content
          // world.
          web::ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {
              web::java_script_features::GetCommonJavaScriptFeature(),
              AutofillFormFeaturesJavaScriptFeature::GetInstance(),
              autofill::FormUtilJavaScriptFeature::GetInstance(),
          }) {}

ProgrammaticFormSubmissionHandlerJavaScriptFeature::
    ~ProgrammaticFormSubmissionHandlerJavaScriptFeature() = default;

std::optional<std::string> ProgrammaticFormSubmissionHandlerJavaScriptFeature::
    GetScriptMessageHandlerName() const {
  return kScriptMessageName;
}

void ProgrammaticFormSubmissionHandlerJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!base::FeatureList::IsEnabled(kAutofillIsolatedWorldForJavascriptIos)) {
    return;
  }

  // Delegate message handling to FormActivityTabHelper.
  FormActivityTabHelper* helper =
      FormActivityTabHelper::GetOrCreateForWebState(web_state);
  helper->OnFormMessageReceived(web_state, message);
}

}  // namespace autofill

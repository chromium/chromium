// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/remote_frame_registration_java_script_feature.h"

#import <optional>

#import "base/feature_list.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_state.h"

namespace autofill {

using FeatureScript = web::JavaScriptFeature::FeatureScript;

// static
RemoteFrameRegistrationJavaScriptFeature*
RemoteFrameRegistrationJavaScriptFeature::GetInstance() {
  static base::NoDestructor<RemoteFrameRegistrationJavaScriptFeature> instance;
  return instance.get();
}

RemoteFrameRegistrationJavaScriptFeature::
    RemoteFrameRegistrationJavaScriptFeature()
    : web::JavaScriptFeature(
          ContentWorldForAutofillJavascriptFeatures(),
          {FeatureScript::CreateWithFilename(
              kRemoteFrameRegistrationScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::
                  kReinjectOnDocumentRecreation)},
          {
              autofill::AutofillFormFeaturesJavaScriptFeature::GetInstance(),
          }) {}

RemoteFrameRegistrationJavaScriptFeature::
    ~RemoteFrameRegistrationJavaScriptFeature() = default;

std::optional<std::string>
RemoteFrameRegistrationJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kRemoteFrameRegistrationMessageHandlerName;
}

void RemoteFrameRegistrationJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAcrossIframesIos) &&
      !base::FeatureList::IsEnabled(kAutofillIsolatedWorldForJavascriptIos)) {
    return;
  }

  if (!message.body() || !message.body()->is_dict()) {
    return;
  }

  if (auto* registrar =
          ChildFrameRegistrar::GetOrCreateForWebState(web_state)) {
    registrar->ProcessRegistrationMessage(message.body());
  }
}

}  // namespace autofill

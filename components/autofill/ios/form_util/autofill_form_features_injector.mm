// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/autofill_form_features_injector.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/web_state.h"

using web::ContentWorld;
using web::WebFrame;
using web::WebFramesManager;
using web::WebState;

namespace {
// Injects feature flags in each WebFrame in `web_state` in the given content
// world.
void SetFeatureFlags(WebState* web_state, ContentWorld content_world) {
  // Inject flags in each existing frames.
  for (auto* web_frame :
       web_state->GetWebFramesManager(content_world)->GetAllWebFrames()) {
    autofill::SetAutofillFormFeatureFlags(web_frame);
  }
}

}  // namespace

namespace autofill {

void SetAutofillFormFeatureFlags(WebFrame* web_frame) {
  AutofillFormFeaturesJavaScriptFeature::GetInstance()
      ->SetAutofillAcrossIframes(
          web_frame,
          base::FeatureList::IsEnabled(features::kAutofillAcrossIframesIos));

  AutofillFormFeaturesJavaScriptFeature::GetInstance()
      ->SetAutofillIsolatedContentWorld(
          web_frame,
          base::FeatureList::IsEnabled(kAutofillIsolatedWorldForJavascriptIos));
}

AutofillFormFeaturesInjector::~AutofillFormFeaturesInjector() = default;

AutofillFormFeaturesInjector::AutofillFormFeaturesInjector(
    WebState* web_state,
    ContentWorld content_world) {
  CHECK(web_state);

  web_state_observation_.Observe(web_state);

  web_frames_manager_observation_.Observe(
      web_state->GetWebFramesManager(content_world));

  SetFeatureFlags(web_state, content_world);
}

void AutofillFormFeaturesInjector::WebStateDestroyed(WebState* web_state) {
  web_frames_manager_observation_.Reset();
  web_state_observation_.Reset();
}

void AutofillFormFeaturesInjector::WebFrameBecameAvailable(
    WebFramesManager* web_frames_manager,
    WebFrame* web_frame) {
  // Inject flags in each new frame.
  SetAutofillFormFeatureFlags(web_frame);
}

}  // namespace autofill

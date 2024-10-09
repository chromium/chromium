// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/autofill_renderer_id_java_script_feature.h"

#import "components/autofill/ios/common/javascript_feature_util.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"

namespace {

const char kScriptName[] = "renderer_id";
}

namespace autofill {

// static
AutofillRendererIDJavaScriptFeature*
AutofillRendererIDJavaScriptFeature::GetInstance() {
  static base::NoDestructor<AutofillRendererIDJavaScriptFeature> instance;
  return instance.get();
}

AutofillRendererIDJavaScriptFeature::AutofillRendererIDJavaScriptFeature()
    : web::JavaScriptFeature(
          ContentWorldForAutofillJavascriptFeatures(),
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetBaseJavaScriptFeature()}) {}

AutofillRendererIDJavaScriptFeature::~AutofillRendererIDJavaScriptFeature() =
    default;

}  // namespace autofill

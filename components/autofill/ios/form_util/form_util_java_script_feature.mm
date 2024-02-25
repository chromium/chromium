// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_util_java_script_feature.h"

#include "base/no_destructor.h"
#include "base/values.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"

namespace {
const char kFillScriptName[] = "fill";
const char kFormScriptName[] = "form";
const char kFeaturesScriptName[] = "autofill_form_features";
}  // namespace

namespace autofill {

// static
FormUtilJavaScriptFeature* FormUtilJavaScriptFeature::GetInstance() {
  static base::NoDestructor<FormUtilJavaScriptFeature> instance;
  return instance.get();
}

FormUtilJavaScriptFeature::FormUtilJavaScriptFeature()
    : web::JavaScriptFeature(
          ContentWorldForAutofillJavascriptFeatures(),
          {FeatureScript::CreateWithFilename(
               kFeaturesScriptName,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames,
               FeatureScript::ReinjectionBehavior::kInjectOncePerWindow),
           FeatureScript::CreateWithFilename(
               kFillScriptName,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames,
               FeatureScript::ReinjectionBehavior::kInjectOncePerWindow),
           FeatureScript::CreateWithFilename(
               kFormScriptName,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames,
               FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetCommonJavaScriptFeature(),
           web::java_script_features::GetMessageJavaScriptFeature()}) {}

FormUtilJavaScriptFeature::~FormUtilJavaScriptFeature() = default;

void FormUtilJavaScriptFeature::SetUpForUniqueIDsWithInitialState(
    web::WebFrame* frame,
    uint32_t next_available_id) {
  CallJavaScriptFunction(
      frame, "fill.setUpForUniqueIDs",
      base::Value::List().Append(static_cast<int>(next_available_id)));
}

void FormUtilJavaScriptFeature::SetAutofillAcrossIframes(web::WebFrame* frame,
                                                         bool enabled) {
  CallJavaScriptFunction(frame,
                         "autofill_form_features.setAutofillAcrossIframes",
                         base::Value::List().Append(enabled));
}

}  // namespace autofill

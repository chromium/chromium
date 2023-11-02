// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_util_java_script_feature.h"

#include "base/no_destructor.h"
#include "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kFillScriptName[] = "fill";
const char kFormScriptName[] = "form";
}  // namespace

namespace autofill {

// static
FormUtilJavaScriptFeature* FormUtilJavaScriptFeature::GetInstance() {
  static base::NoDestructor<FormUtilJavaScriptFeature> instance;
  return instance.get();
}

FormUtilJavaScriptFeature::FormUtilJavaScriptFeature()
    : web::JavaScriptFeature(
          // TODO(crbug.com/1175793): Move autofill code to kAnyContentWorld
          // once all scripts are converted to JavaScriptFeatures.
          ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
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
  std::vector<base::Value> parameters;
  parameters.emplace_back(static_cast<int>(next_available_id));
  CallJavaScriptFunction(frame, "fill.setUpForUniqueIDs", parameters);
}

}  // namespace autofill

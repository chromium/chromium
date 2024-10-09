// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_util_java_script_feature.h"

#import "base/no_destructor.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"

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
          // Form submission detection hook in the page content world
          // requires fill.ts and form.ts. That is why injection in both
          // worlds is required.
          web::ContentWorld::kAllContentWorlds,
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
          {
              web::java_script_features::GetCommonJavaScriptFeature(),
              web::java_script_features::GetMessageJavaScriptFeature(),
          }) {}

FormUtilJavaScriptFeature::~FormUtilJavaScriptFeature() = default;

}  // namespace autofill

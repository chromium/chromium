// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/cross_content_world_util_java_script_feature.h"

#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"

namespace {

const char kFillScriptName[] = "cross_content_world_fill_util";

}

namespace autofill {

// static
CrossContentWorldUtilJavaScriptFeature*
CrossContentWorldUtilJavaScriptFeature::GetInstance() {
  static base::NoDestructor<CrossContentWorldUtilJavaScriptFeature> instance;
  return instance.get();
}

CrossContentWorldUtilJavaScriptFeature::CrossContentWorldUtilJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kAllContentWorlds,
          {FeatureScript::CreateWithFilename(
              kFillScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetBaseJavaScriptFeature()}) {}

CrossContentWorldUtilJavaScriptFeature::
    ~CrossContentWorldUtilJavaScriptFeature() = default;

}  // namespace autofill

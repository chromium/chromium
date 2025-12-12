// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/renderer_id_test_util.h"

#import "ios/web/public/js_messaging/java_script_feature.h"

namespace autofill::test {

std::unique_ptr<web::JavaScriptFeature>
CreateRendererIdTestJavaScriptFeature() {
  web::JavaScriptFeature::FeatureScript renderer_id_test_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "renderer_id_test",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
          web::JavaScriptFeature::FeatureScript::ReinjectionBehavior::
              kInjectOncePerWindow);

  return std::make_unique<web::JavaScriptFeature>(
      web::ContentWorld::kIsolatedWorld,
      std::vector<web::JavaScriptFeature::FeatureScript>{
          renderer_id_test_script});
}

}  // namespace autofill::test

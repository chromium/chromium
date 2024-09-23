// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_CROSS_CONTENT_WORLD_UTIL_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_CROSS_CONTENT_WORLD_UTIL_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace autofill {

// Injects Autofill utils that need to live both in the page and the isolated
// content world.
class CrossContentWorldUtilJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static CrossContentWorldUtilJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<CrossContentWorldUtilJavaScriptFeature>;

  CrossContentWorldUtilJavaScriptFeature();
  ~CrossContentWorldUtilJavaScriptFeature() override;

  CrossContentWorldUtilJavaScriptFeature(
      const CrossContentWorldUtilJavaScriptFeature&) = delete;
  CrossContentWorldUtilJavaScriptFeature& operator=(
      const CrossContentWorldUtilJavaScriptFeature&) = delete;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_CROSS_CONTENT_WORLD_UTIL_JAVA_SCRIPT_FEATURE_H_

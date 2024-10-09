// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_RENDERER_ID_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_RENDERER_ID_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace autofill {

// Injects Autofill functions for setting stable renderer IDs to html forms and
// fields.
class AutofillRendererIDJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static AutofillRendererIDJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<AutofillRendererIDJavaScriptFeature>;
  // Friend test fixture so it can create instances of this class. This JS
  // feature is injected in different content worlds depending on a feature
  // flag. Tests need to create new instances of the JS feature when the feature
  // flag changes.
  // TODO(crbug.com/359538514): Remove friend once isolated world for Autofill
  // is launched.
  friend class FillJsTest;
  friend class TestAutofillJavaScriptFeatureContainer;

  AutofillRendererIDJavaScriptFeature();
  ~AutofillRendererIDJavaScriptFeature() override;

  AutofillRendererIDJavaScriptFeature(
      const AutofillRendererIDJavaScriptFeature&) = delete;
  AutofillRendererIDJavaScriptFeature& operator=(
      const AutofillRendererIDJavaScriptFeature&) = delete;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_RENDERER_ID_JAVA_SCRIPT_FEATURE_H_

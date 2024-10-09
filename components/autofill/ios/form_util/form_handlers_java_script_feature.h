// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_HANDLERS_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_HANDLERS_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {

class AutofillFormFeaturesJavaScriptFeature;
class AutofillRendererIDJavaScriptFeature;

// Registers listeners that are used to handle forms, enabling autofill and the
// replacement method to dismiss the keyboard needed because of the Autofill
// keyboard accessory.
class FormHandlersJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static FormHandlersJavaScriptFeature* GetInstance();

  // Toggles tracking form related changes in the frame. Will allow batching an
  // added form activity and a removed form activity when `allowBatching` is
  // true.
  void TrackFormMutations(web::WebFrame* frame, int mutation_tracking_delay);

  // Toggles tracking the source of the input events in the frame.
  void ToggleTrackingUserEditedFields(web::WebFrame* frame,
                                      bool track_user_edited_fields);

 private:
  friend class base::NoDestructor<FormHandlersJavaScriptFeature>;
  // TODO(crbug.com/359538514): Remove friend once isolated world for Autofill
  // is launched.
  friend class TestAutofillJavaScriptFeatureContainer;

  // web::JavaScriptFeature
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  FormHandlersJavaScriptFeature();
  ~FormHandlersJavaScriptFeature() override;

  FormHandlersJavaScriptFeature(const FormHandlersJavaScriptFeature&) = delete;
  FormHandlersJavaScriptFeature& operator=(
      const FormHandlersJavaScriptFeature&) = delete;

  // For testing.
  // TODO(crbug.com/359538514): Remove test constructor once isolated world for
  // Autofill is launched.
  FormHandlersJavaScriptFeature(
      AutofillFormFeaturesJavaScriptFeature*
          autofill_form_features_java_script_feature,
      AutofillRendererIDJavaScriptFeature* renderer_id_feature);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_HANDLERS_JAVA_SCRIPT_FEATURE_H_

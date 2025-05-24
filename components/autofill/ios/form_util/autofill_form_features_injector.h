// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_FORM_FEATURES_INJECTOR_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_FORM_FEATURES_INJECTOR_H_

#import "base/scoped_observation.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer.h"

namespace web {
class WebState;
class WebFrame;
}  // namespace web

namespace autofill {

// Injects Autofill form features flags in `web_frame`.
void SetAutofillFormFeatureFlags(web::WebFrame* web_frame);

// Sets form feature flags in the renderer so injected scripts can query them.
class AutofillFormFeaturesInjector : public web::WebStateObserver,
                                     public web::WebFramesManager::Observer {
 public:
  // Creates an AutofillFormFeaturesInjector for setting feature flags in all
  // frames in a given content world.
  AutofillFormFeaturesInjector(web::WebState* web_state,
                               web::ContentWorld content_world);
  ~AutofillFormFeaturesInjector() override;

  AutofillFormFeaturesInjector(const AutofillFormFeaturesInjector&) = delete;
  AutofillFormFeaturesInjector operator=(const AutofillFormFeaturesInjector&) =
      delete;

  // web::WebStateObserver
  void WebStateDestroyed(web::WebState* web_state) override;

  // web::WebFramesManager::Observer
  void WebFrameBecameAvailable(web::WebFramesManager* web_frames_manager,
                               web::WebFrame* web_frame) override;

 private:
  // Observes WebState destruction to remove WebFramesManager observations.
  base::ScopedObservation<web::WebState, AutofillFormFeaturesInjector>
      web_state_observation_{this};
  // Observes the WebFramesManager for a given content world.
  base::ScopedObservation<web::WebFramesManager, AutofillFormFeaturesInjector>
      web_frames_manager_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_FORM_FEATURES_INJECTOR_H_

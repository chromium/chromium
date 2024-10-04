// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_OBSERVER_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/memory/raw_ptr.h"
#include "components/autofill/ios/form_util/form_activity_observer.h"

@protocol FormActivityObserver<NSObject>
@optional
// Invoked by FormActivityObserverBridge::FormActivity.
- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame;

// Invoked by FormActivityObserverBridge::DidSubmitDocument.
- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormData:(const autofill::FormData&)formData
                   hasUserGesture:(BOOL)hasUserGesture
                          inFrame:(web::WebFrame*)frame;

// Invoked by FormActivityObserverBridge::FormRemoved.
- (void)webState:(web::WebState*)webState
    didRegisterFormRemoval:(const autofill::FormRemovalParams&)params
                   inFrame:(web::WebFrame*)frame;

@end

namespace autofill {

// Use this class to be notified of the form activity in an Objective-C class.
// Implement the |FormActivityObserver| activity protocol and create a strong
// member FormActivityObserverBridge
// form_activity_obserber_bridge_ =
//     std::make_unique<FormActivityObserverBridge>(web_state, self);
// It is the responsibility of the owner class to delete this bridge if the
// web_state becomes invalid.
class FormActivityObserverBridge : public FormActivityObserver {
 public:
  // |owner| will not be retained.
  FormActivityObserverBridge(web::WebState* web_state,
                             id<FormActivityObserver> owner);

  FormActivityObserverBridge(const FormActivityObserverBridge&) = delete;
  FormActivityObserverBridge& operator=(const FormActivityObserverBridge&) =
      delete;

  ~FormActivityObserverBridge() override;

  // FormActivityObserver overrides:
  void FormActivityRegistered(web::WebState* web_state,
                              web::WebFrame* sender_frame,
                              const FormActivityParams& params) override;

  void DocumentSubmitted(web::WebState* web_state,
                         web::WebFrame* sender_frame,
                         const FormData& form_data,
                         bool has_user_gesture) override;

  void FormRemoved(web::WebState* web_state,
                   web::WebFrame* sender_frame,
                   const FormRemovalParams& params) override;

 private:
  raw_ptr<web::WebState> web_state_ = nullptr;
  __weak id<FormActivityObserver> owner_ = nil;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_OBSERVER_BRIDGE_H_

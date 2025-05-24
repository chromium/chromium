// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_OBSERVER_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_OBSERVER_H_

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {

struct FormActivityParams;
struct FormRemovalParams;
class FormData;

// Interface for observing form activity.
// It is the responsibility of the observer to unregister if the web_state
// becomes invalid.
class FormActivityObserver {
 public:
  FormActivityObserver() = default;

  FormActivityObserver(const FormActivityObserver&) = delete;
  FormActivityObserver& operator=(const FormActivityObserver&) = delete;

  virtual ~FormActivityObserver() = default;

  // Called when the user is typing on a form field in the main frame or in a
  // same-origin iframe. |params.input_missing| is indicating if there is any
  // error when parsing the form field information.
  // |sender_frame| is the WebFrame that sent the form activity message.
  virtual void FormActivityRegistered(web::WebState* web_state,
                                      web::WebFrame* sender_frame,
                                      const FormActivityParams& params) {}

  // Called on form submission in the main frame or in a same-origin iframe.
  // |has_user_gesture| is true if the user interacted with the page.
  // |form_data| contains information on the form that has been submitted.
  // |sender_frame| is the WebFrame that sent the form submission message.
  virtual void DocumentSubmitted(web::WebState* web_state,
                                 web::WebFrame* sender_frame,
                                 const FormData& form_data,
                                 bool has_user_gesture) {}

  // Called when the form is removed in the main frame or in the same-origin
  // iframe. |params.input_missing| is indicating if there is any
  // error when parsing the form field information.
  // |sender_frame| is the WebFrame that sent the form activity message.
  virtual void FormRemoved(web::WebState* web_state,
                           web::WebFrame* sender_frame,
                           const FormRemovalParams& params) {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_OBSERVER_H_

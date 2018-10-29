// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_OBSERVER_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_OBSERVER_H_

#include <string>

#include "base/macros.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {

struct FormActivityParams;

// Interface for observing form activity.
// It is the responsibility of the observer to unregister if the web_state
// becomes invalid.
class FormActivityObserver {
 public:
  FormActivityObserver() {}
  virtual ~FormActivityObserver() {}

  // Called when the user is typing on a form field in the main frame or in a
  // same-origin iframe. |params.input_missing| is indicating if there is any
  // error when parsing the form field information.
  // |sender_frame| is the WebFrame that sent the form activity message.
  // |sender_frame| can be null if frame messaging is not enabled (see
  // web::WebState::ScriptCommandCallback comment for details).
  virtual void FormActivityRegistered(web::WebState* web_state,
                                      web::WebFrame* sender_frame,
                                      const FormActivityParams& params) {}

  // Called on form submission in the main frame or in a same-origin iframe.
  // |has_user_gesture| is true if the user interacted with the page.
  // |form_in_main_frame| is true if the submitted form is hosted in the main
  // frame.
  // |form_data| contains information on the form that has been submitted.
  // It is in a JSON format and can be decoded by autofill::ExtractFormsData.
  // It is a list (for compatibility reason) containing 0 or 1 dictionary.
  // The dictionary has some element containing some form attributes (HTML or
  // computed ('name', 'action', 'is_formless_checkout'...) and a 'field'
  // element containing a list of dictionaries, each representing a field of the
  // form and contianing some attributes ('name', 'type',...).
  // |sender_frame| is the WebFrame that sent the form submission message.
  // |sender_frame| can be null if frame messaging is not enabled (see
  // web::WebState::ScriptCommandCallback comment for details).
  // TODO(crbug.com/881811): remove |form_in_main_frame| once frame messaging is
  // fully enabled.
  // TODO(crbug.com/881816): Update comment once WebFrame cannot be null.
  virtual void DocumentSubmitted(web::WebState* web_state,
                                 web::WebFrame* sender_frame,
                                 const std::string& form_name,
                                 const std::string& form_data,
                                 bool has_user_gesture,
                                 bool form_in_main_frame) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FormActivityObserver);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_OBSERVER_H_

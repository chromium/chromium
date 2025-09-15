// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_FIELD_DETECTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_FIELD_DETECTOR_H_

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// Utility class to observe if a WebContents (with all of its frames)
// - moves from having 0 OTP fields to >0 OTP fields (only focusable fields are
//   considered)
// - moves from having >0 OTP fields to 0 OTP fields
// - observes an OTP from submission
//
// Internally the tracking happens at a form level (ie. checking if any form
// contains at least one OTP field).
//
// OtpFieldDetector is an abstract base class that needs to be notified about
// new and gone OTP fields by a concrete implementation. See
// `ContentOtpFieldDetector` for such an implementation for content/ platforms.
// It is owned by the AutofillClient and exists once per WebContents.
class OtpFieldDetector {
 public:
  using OtpFieldsDetectedCallback = base::RepeatingCallback<void()>;
  using OtpFieldsSubmittedCallback = base::RepeatingCallback<void()>;

  OtpFieldDetector(const OtpFieldDetector& other) = delete;
  OtpFieldDetector& operator=(const OtpFieldDetector& other) = delete;
  virtual ~OtpFieldDetector();

  // Registers a callback that is called when a WebContents transitions from 0
  // OTP fields to >0 OTP field, where OTP fields are considered across all
  // frames.
  [[nodiscard]] base::CallbackListSubscription
  RegisterOtpFieldsDetectedCallback(OtpFieldsDetectedCallback callback);

  // Registers a callback that is called when a WebContents
  // - transitions from >0 OTP fields to 0 OTP field, where OTP fields are
  //   considered across all frames.
  // - observes an OTP form submission.
  // May be called twice in case a page has two OTP fields, one is submitted and
  // the other is removed.
  [[nodiscard]] base::CallbackListSubscription
  RegisterOtpFieldsSubmittedCallback(OtpFieldsSubmittedCallback callback);

  // Returns whether at present any frame of the WebContents contains at least
  // one OTP field.
  bool IsOtpFieldPresent() const;

 protected:
  // Protected to ensure that only derived classes can be instantiated.
  OtpFieldDetector();

  // Functions that add and remove `form_id` to/from `forms_with_otps_` and
  // notify the registered callbacks if the number of forms with OTP fields goes
  // from 0 to >1 or vice versa. `form_id` is always a form that contains at
  // least one OTP field.
  void AddFormAndNotifyIfNecessary(FormGlobalId form_id);
  // Remove is also called if a form is submitted but remains in the DOM.
  void RemoveFormAndNotifyIfNecessary(FormGlobalId form_id);

 private:
  base::flat_set<FormGlobalId> forms_with_otps_;

  base::RepeatingCallbackList<OtpFieldsDetectedCallback::RunType>
      callback_list_otp_fields_detected_;
  base::RepeatingCallbackList<OtpFieldsSubmittedCallback::RunType>
      callback_list_otp_fields_submitted_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_FIELD_DETECTOR_H_

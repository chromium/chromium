// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_FIELD_DETECTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_FIELD_DETECTOR_H_

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillClient;

// Utility class to observe if a WebContents (with all of its frames)
// - moves from having 0 OTP fields to >0 OTP fields (only focusable fields are
//   considered)
// - moves from having >0 OTP fields to 0 OTP fields
// - observes an OTP from submission
//
// Internally the tracking happens at a form level (ie. checking if any form
// contains at least one OTP field).
//
// Owned by the AutofillClient and exists once per WebContents.
class OtpFieldDetector : public AutofillManager::Observer {
 public:
  using OtpFieldsDetectedCallback = base::RepeatingClosure;
  using OtpFieldsSubmittedCallback = base::RepeatingClosure;

  explicit OtpFieldDetector(AutofillClient* client);
  OtpFieldDetector(const OtpFieldDetector& other) = delete;
  OtpFieldDetector& operator=(const OtpFieldDetector& other) = delete;
  ~OtpFieldDetector() override;

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

  // AutofillManager::Observer:

  // `OnFieldTypesDetermined` informs us incrementally about the discovery of
  // OTP fields. This is called once when the heuristic classifications are
  // available and once again when the server classifications are available.
  // If a server overrides an heuristically identified OTP field with
  // UKNOWN_TYPE, we temporarily report the presence of an OTP.
  void OnFieldTypesDetermined(AutofillManager& manager,
                              FormGlobalId form,
                              FieldTypeSource source) override;
  // We use `OnAfterFormsSeen` in addition to `OnAfterFormSubmitted` to identify
  // the removal of forms because it's possible that an OTP form is removed
  // from the DOM (because it became irrelevant) w/o being submitted.
  void OnAfterFormsSeen(AutofillManager& manager,
                        base::span<const FormGlobalId> updated_forms,
                        base::span<const FormGlobalId> removed_forms) override;
  // We use `OnAfterFormSubmitted` in addition to `OnAfterFormsSeen` because
  // it's possible that a form is submitted but remains (invisible) in the DOM.
  void OnAfterFormSubmitted(AutofillManager& manager,
                            const FormData& form) override;
  // If an AutofillManager changes it's LifecycleState away from active, that
  // means that a navigation has happened and the OTP field is not visible
  // anymore. If the LifecycleState becomes active, the user either navigated to
  // a new document or navigated back in the forward/backward cache. In this
  // case we may bring forms back.
  // Navigations are considered because here because not all OTP forms are
  // submitted and we would not notice the disappearance of forms due to
  // navigations without this event.
  void OnAutofillManagerStateChanged(
      AutofillManager& manager,
      AutofillDriver::LifecycleState previous,
      AutofillDriver::LifecycleState current) override;

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

  ScopedAutofillManagersObservation autofill_manager_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_FIELD_DETECTOR_H_

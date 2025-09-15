// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_CONTENT_OTP_FIELD_DETECTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_CONTENT_OTP_FIELD_DETECTOR_H_

#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"

namespace autofill {

class ContentAutofillClient;

// Utility class to observe if a WebContents (with all of its frames)
// - moves from having 0 OTP fields to >0 OTP fields (only focusable fields are
//   considered)
// - moves from having >0 OTP fields to 0 OTP fields
// - observes an OTP from submission
//
// See `class OtpFieldDetector` as well. This is the content/ specific
// implementation of the OtpFieldDetecctor.
//
// Owned by the AutofillClient and exists once per WebContents.
//
// TODO(crbug.com/415273270) We can now merge this with the OtpFieldDetector
// base class because ScopedAutofillManagersObservation accepts an
// AutofillClient.
class ContentOtpFieldDetector : public OtpFieldDetector,
                                public AutofillManager::Observer {
 public:
  explicit ContentOtpFieldDetector(ContentAutofillClient* client);
  ~ContentOtpFieldDetector() override;

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

 private:
  ScopedAutofillManagersObservation autofill_manager_observation_{this};
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_CONTENT_OTP_FIELD_DETECTOR_H_

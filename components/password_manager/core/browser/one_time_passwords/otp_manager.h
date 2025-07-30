// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/integrators/password_manager/otp_suggestion_delegate.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace password_manager {

class OtpFormManager;
class PasswordManagerClient;

// A class in charge of handling one time passwords, one per tab.
class OtpManager : public autofill::OtpSuggestionDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnOtpFieldDetected(OtpFormManager* form_manager) = 0;
  };

  explicit OtpManager(PasswordManagerClient* client);

  ~OtpManager() override;

  // Processes the classification model predictions received via Autofill.
  void ProcessClassificationModelPredictions(
      const autofill::FormData& form,
      const base::flat_map<autofill::FieldGlobalId, autofill::FieldType>&
          field_predictions);

  // Processes the server predictions.
  void ProcessServerPredictions(
      const autofill::FormData& form,
      const base::flat_map<autofill::FieldGlobalId,
                           autofill::AutofillType::ServerPrediction>&
          field_predictions);

  // OtpSuggestionDelegate implementation
  bool IsFieldEligibleForOtpFilling(
      const autofill::FormGlobalId& form_id,
      const autofill::FieldGlobalId& field_id) const override;
  void GetOtpSuggestions(const autofill::FormGlobalId& form_id,
                         const autofill::FieldGlobalId& field_id,
                         base::OnceCallback<void(std::vector<std::string>)>
                             callback) const override;

  // Called by the client when the renderer frame identified by `frame_token` is
  // deleted.
  void OnRenderFrameDeleted(const autofill::LocalFrameToken& frame_token);

  // Called by the client when the main frame finishes navigating away from the
  // current page.
  void OnDidFinishNavigationInMainFrame();

  // Called by the client when an iframe finishes navigating away from the
  // current page.
  void OnDidFinishNavigationInIframe(
      const autofill::LocalFrameToken& frame_token);

  const base::flat_map<autofill::FormGlobalId, std::unique_ptr<OtpFormManager>>&
  form_managers() const {
    return form_managers_;
  }

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  // Returns a manager for a form, if it exists, or nullptr otherwise.
  OtpFormManager* GetManagerForForm(
      const autofill::FormGlobalId& form_id) const;

  // Removes form managers managing OTP forms in a frame identified by
  // `frame_token`.
  void CleanFormManagersForTheFrame(
      const autofill::LocalFrameToken& frame_token);

  // The client that owns this class and is guaranteed to outlive it.
  const raw_ptr<PasswordManagerClient> client_;

  // Managers managing individual forms.
  // unique_ptrs are used to store form managers to allow moving the objects
  // without invalidating weak_ptrs to form managers.
  base::flat_map<autofill::FormGlobalId, std::unique_ptr<OtpFormManager>>
      form_managers_;

  base::ObserverList<Observer> observers_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_MANAGER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_

#include <map>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_form_finder.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"

namespace affiliations {
struct Facet;
}  // namespace affiliations

namespace password_manager {
class PasswordManagerInterface;
}  // namespace password_manager

namespace actor_login {

class ActorLoginFormFinder;

// Fills a given credential into the matching signin form if one exists.
class ActorLoginCredentialFiller {
 public:
  using IsTaskInFocus = base::RepeatingCallback<bool()>;

  ActorLoginCredentialFiller(
      const url::Origin& main_frame_origin,
      const Credential& credential,
      bool should_store_permission,
      password_manager::PasswordManagerClient* client,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      IsTaskInFocus is_task_in_focus,
      LoginStatusResultOrErrorReply callback);
  ~ActorLoginCredentialFiller();

  ActorLoginCredentialFiller(const ActorLoginCredentialFiller&) = delete;
  ActorLoginCredentialFiller& operator=(const ActorLoginCredentialFiller&) =
      delete;

  // Attempts to fill the credential provided in the constructor.
  // `password_manager` is used to find the signin form.
  void AttemptLogin(
      password_manager::PasswordManagerInterface* password_manager);

 private:
  enum class FieldType { kUsername, kPassword };

  // Called when the affiliations for `credential_.request_origin` have been
  // retrieved. `results` contains facets affiliated with the
  // `credential_.request_origin`.
  void OnAffiliationsReceived(const std::vector<affiliations::Facet>& results,
                              bool success);

  void FetchEligibleForms(
      base::OnceCallback<
          void(std::vector<password_manager::PasswordFormManager*>)>
          on_forms_retrieved_cb);

  // Should always be called synchronously.
  void ProcessRetrievedForms(
      std::vector<password_manager::PasswordFormManager*> eligible_managers);

  // Checks if device reauthentication is required before filling.
  // If required, triggers reauthentication and, upon success, re-fetches
  // eligible forms to ensure freshness before filling all of them.
  // If not required, proceeds directly to filling all eligible fields in
  // `eligible_managers`.
  void MaybeReauthAndFillAllEligibleFields(
      std::vector<password_manager::PasswordFormManager*> eligible_managers,
      const password_manager::PasswordForm& stored_credential);

  // Triggers the device reauthentication flow.
  // `on_reauth_cb` is executed only if reauthentication is successful.
  // If reauthentication fails, the filling process is aborted and an error
  // is reported via `callback_`.
  void AttemptReauth(base::OnceClosure on_reauth_cb);

  // Retrieves the full data of a saved credential for the form managed
  // by `signin_form_manager` corresponding to `credential_`.
  const password_manager::PasswordForm* GetMatchingStoredCredential(
      const password_manager::PasswordFormManager& signin_form_manager);

  // Reauthenticates the user before filling.
  void ReauthenticateAndFill(base::OnceClosure fill_form_cb);

  // Called after the reauthentication step with the result of the reauth
  // operation. Invokes `fill_form_cb` if authentication was successful.
  void OnDeviceReauthCompleted(base::OnceClosure fill_form_cb,
                               bool authenticated);

  // Fills all eligible fields with `stored_credential.password_value` and
  // `stored_credential.username_value`.
  void FillAllEligibleFields(
      const password_manager::PasswordForm& stored_credential,
      bool should_skip_iframes,
      std::vector<password_manager::PasswordFormManager*> eligible_managers);

  // Fills the field of `type` identified by `field_renderer_id` within the
  // `driver`'s frame with `value`. `closure` will be called to signal
  // completion at the very end of the flow.
  void FillField(password_manager::PasswordManagerDriver* driver,
                 autofill::FormGlobalId form_global_id,
                 autofill::FieldRendererId field_renderer_id,
                 const std::u16string& value,
                 FieldType type,
                 base::OnceClosure closure);

  // Called with the success status of filling the respective field.
  void ProcessSingleFillingResult(autofill::FormGlobalId form_global_id,
                                  FieldType field_type,
                                  autofill::FieldRendererId field_id,
                                  bool success);

  // Called when all filling operations have finished. Invokes `callback_`
  // with the result based on `username_filled_` and `password_filled_`.
  void OnFillingDone();

  // Populates data into `attempt_login_logs_` at the end of credential
  // filling.
  void BuildAttemptLoginOutcome(AttemptLoginOutcomeMqls outcome);

  // The origin of the primary main frame.
  const url::Origin origin_;

  // The credential to fill in either the primary main frame or the frame
  // matching the `origin_`.
  const Credential credential_;

  // Whether user chose to always allow actor login to use `credential_`
  const bool should_store_permission_ = false;

  // Populated with the aggregated results of the calls to fill.
  bool username_filled_ = false;
  bool password_filled_ = false;

  // The result of filling eligible login forms. The key is the global id of
  // the form.
  absl::flat_hash_map<
      autofill::FormGlobalId,
      optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails::
          FillingFormResult>
      filling_results_;

  // Safe to access from everywhere apart from the destructor.
  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  // Helper class that sends MQLS logs about full actor login attempt
  // (GetCredentials + AttemptLogin). Owned by AttemptLoginTool.
  // TODO(crbug.com/460025687): Use raw_ptr instead.
  base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger_;

  // Logs entry to be given to `mqls_logger` when the request ends.
  optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails
      attempt_login_logs_;

  // Used to compute the request duration. Excludes reauth duration.
  base::TimeTicks start_time_;

  // Used to compute the reauth duration if it was initiated. The duration is
  // subtracted from the request duration.
  std::optional<base::TimeTicks> reauth_start_time_;

  // Helper object for finding login forms.
  std::unique_ptr<ActorLoginFormFinder> login_form_finder_;

  // Checks whether the UI relevant to the actor login task is in focus.
  IsTaskInFocus is_task_in_focus_;

  // The callback to call with the result of the login attempt.
  LoginStatusResultOrErrorReply callback_;

  // Used to reauthenticate the user before filling
  // the credential.
  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to `ActorLoginCredentialFiller` are invalidated before
  // its member variables' destructors are executed, rendering them invalid.
  base::WeakPtrFactory<ActorLoginCredentialFiller> weak_ptr_factory_{this};
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_CREDENTIAL_FILLER_H_

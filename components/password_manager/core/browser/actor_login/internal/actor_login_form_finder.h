// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FORM_FINDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FORM_FINDER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class GURL;

namespace password_manager {
struct PasswordForm;
class PasswordFormManager;
class PasswordManagerClient;
class PasswordManagerDriver;
}  // namespace password_manager

namespace url {
class Origin;
}  // namespace url

namespace autofill {
class FormRendererId;
}

namespace actor_login {

struct FormFinderResult {
  FormFinderResult();
  FormFinderResult(
      std::vector<password_manager::PasswordFormManager*> eligible_managers,
      std::vector<
          optimization_guide::proto::ActorLoginQuality_ParsedFormDetails>
          parsed_forms_details);

  ~FormFinderResult();

  FormFinderResult(const FormFinderResult&);
  FormFinderResult& operator=(const FormFinderResult&);
  FormFinderResult(FormFinderResult&&);
  FormFinderResult& operator=(FormFinderResult&&);

  std::vector<password_manager::PasswordFormManager*> eligible_managers;
  std::vector<optimization_guide::proto::ActorLoginQuality_ParsedFormDetails>
      parsed_forms_details;
};

// Class for field types that can be found in a form.
// Defined here so it can be used by helper functions in the implementation.
enum class LoginFieldType {
  kUsername,
  kPassword,
  kNewPassword,
};

// Helper class to find all the login forms.
class ActorLoginFormFinder {
 public:
  // A callback that receives all eligible login form managers found on the
  // page.
  using EligibleManagersCallback = base::OnceCallback<void(FormFinderResult)>;
  using DriverFormKey =
      std::pair<autofill::FormRendererId,
                base::WeakPtr<password_manager::PasswordManagerDriver>>;

  explicit ActorLoginFormFinder(
      password_manager::PasswordManagerClient* client);
  ~ActorLoginFormFinder();

  ActorLoginFormFinder(const ActorLoginFormFinder&) = delete;
  ActorLoginFormFinder& operator=(const ActorLoginFormFinder&) = delete;

  // Sets the form data in `form_data_proto` using the data of the given `form`.
  static void SetFormData(
      optimization_guide::proto::ActorLoginQuality_FormData& form_data_proto,
      const password_manager::PasswordForm& form);

  // Extracts the site or app origin (scheme, host, port) from a URL as a
  // string.
  static std::u16string GetSourceSiteOrAppFromUrl(const GURL& url);

  // Finds the most suitable `PasswordFormManager` from the list.
  // It prioritizes forms in the primary main frame.
  static password_manager::PasswordFormManager* GetSigninFormManager(
      const std::vector<password_manager::PasswordFormManager*>&
          eligible_managers);

  // Retrieves all `PasswordFormManager`s for the given `origin` that contain a
  // valid login form, along with the
  // `optimization_guide::proto::ActorLoginQuality_ParsedFormDetails` for all
  // parsed forms.
  FormFinderResult GetEligibleLoginFormManagers(const url::Origin& origin);

  // Asynchronously finds all `PasswordFormManager`s that are associated with
  // `origin` and have a valid parsed login form. Invokes `callback` with the
  // results.
  void GetEligibleLoginFormManagersAsync(const url::Origin& origin,
                                         EligibleManagersCallback callback);

 private:
  // Checks if the `PasswordForm` visible fields correspond to a login form.
  // It should always be called for forms that are in a valid frame
  // and origin.
  void IsLoginFormAsync(
      const password_manager::PasswordForm& form,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      base::OnceCallback<void(bool)> callback);

  // Callback executed when the visibility checks for a specific form are done.
  // It populates the `form_details`, records  the timing,
  // and replies to callback with whether the form is a login form.
  void OnVisibilityChecksComplete(
      optimization_guide::proto::ActorLoginQuality_ParsedFormDetails
          form_details,
      base::TimeTicks start_time,
      base::OnceCallback<void(bool)> callback,
      std::vector<std::pair<LoginFieldType, bool>> results);

  // Callback for when all candidate forms have been checked for eligibility.
  // It filters the candidates and passes the final eligible list to `callback`.
  void OnAllEligibleChecksCompleted(
      EligibleManagersCallback callback,
      std::vector<std::pair<
          std::pair<autofill::FormRendererId,
                    base::WeakPtr<password_manager::PasswordManagerDriver>>,
          bool>> results);

  const raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  // Form details of all the forms found in a single
  // `GetEligibleLoginFormManagersAsync` call.
  std::vector<optimization_guide::proto::ActorLoginQuality_ParsedFormDetails>
      parsed_forms_details_;

  base::WeakPtrFactory<ActorLoginFormFinder> weak_ptr_factory_{this};
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FORM_FINDER_H_

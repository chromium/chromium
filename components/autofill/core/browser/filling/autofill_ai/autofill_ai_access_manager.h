// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_AUTOFILL_AI_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_AUTOFILL_AI_ACCESS_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace device_reauth {
class DeviceAuthenticator;
}  // namespace device_reauth

namespace url {
class Origin;
}  // namespace url

namespace autofill {

class BrowserAutofillManager;

// Manages authentication and server fetching for Autofill AI entities.
// Owned by `BrowserAutofillManager`.
class AutofillAiAccessManager {
 public:
  enum class FailureReason {
    kReauthFailed,
    kFetchFailed,
  };

  using OnEntityInstanceFetchedCallback = base::OnceCallback<void(
      base::expected<EntityInstance, FailureReason> result,
      bool reauth_attempted)>;

  explicit AutofillAiAccessManager(BrowserAutofillManager* manager);
  AutofillAiAccessManager(const AutofillAiAccessManager&) = delete;
  AutofillAiAccessManager& operator=(const AutofillAiAccessManager&) = delete;
  virtual ~AutofillAiAccessManager();

  // Fetches and authenticates the given `entity` through the following flow:
  // 1. Triggers a best-effort device re-authentication before unmasking if
  //    authentication conditions are met.
  // 2. If re-authentication succeeds (or isn't required), fetches the unmasked
  //    server entity from the wallet server if the entity is a masked server
  //    entity.
  // 3. Invokes `callback` with the final entity on success, or with a
  //    `FailureReason` if re-authentication or unmasking fails.
  //
  // Returns a boolean indicating if the operation is asynchronous (requiring
  // server-side fetching or device re-authentication). Only one flow can be
  // active at any given time; starting a new one cancels the previous one.
  // When a pending flow is cancelled, its callback is not executed.
  virtual bool FetchEntityInstance(EntityInstance entity,
                                   bool will_fill_sensitive_info,
                                   OnEntityInstanceFetchedCallback callback);

  // Cancels any pending authentication or fetch operations and invalidates all
  // pending callbacks.
  virtual void Reset();

 private:
  friend class AutofillAiAccessManagerTestApi;

  // Handles optional device re-authentication before unmasking. Invokes
  // `callback` with the unmasked entity if authentication succeeds or isn't
  // needed, or with the failure reason if it fails.
  void MaybeAuthenticate(EntityInstance entity,
                         bool should_reauth,
                         OnEntityInstanceFetchedCallback callback);

  // Triggers user authentication and runs `callback` with an authentication
  // result on completion.
  void Authenticate(const url::Origin& origin,
                    base::OnceCallback<void(bool)> callback);

  // Handles optional server-side fetching of the unmasked server entity.
  // Invokes `callback` with the unmasked entity, or the failure reason if it
  // fails.
  void MaybeUnmaskServerEntity(
      OnEntityInstanceFetchedCallback callback,
      base::expected<EntityInstance, FailureReason> result,
      bool reauth_attempted);

  // The owning manager.
  const raw_ref<BrowserAutofillManager> manager_;

  // Used to re-authenticate the user before fetching.
  // This authenticator is created when this manager is instantiated and lives
  // for the lifetime of the class.
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator_;

  base::WeakPtrFactory<AutofillAiAccessManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_AUTOFILL_AI_ACCESS_MANAGER_H_

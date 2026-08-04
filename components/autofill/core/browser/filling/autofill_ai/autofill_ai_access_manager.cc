// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/device_authenticator.h"
#include "url/origin.h"

namespace autofill {

AutofillAiAccessManager::AutofillAiAccessManager(
    BrowserAutofillManager* manager)
    : manager_(CHECK_DEREF(manager)) {}

AutofillAiAccessManager::~AutofillAiAccessManager() = default;

bool AutofillAiAccessManager::FetchEntityInstance(
    EntityInstance entity,
    bool will_fill_sensitive_info,
    OnEntityInstanceFetchedCallback callback) {
  // Invalidate any pending operations from prior flows, ensuring that only one
  // flow is active at a time.
  Reset();

  // This ensures that if the manager is reset during any asynchronous phase,
  // the final callback is safely ignored and never executed.
  callback = base::BindOnce(
      [](base::WeakPtr<AutofillAiAccessManager> self,
         OnEntityInstanceFetchedCallback callback,
         base::expected<EntityInstance, FailureReason> result,
         bool reauth_attempted) {
        if (self) {
          std::move(callback).Run(std::move(result), reauth_attempted);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  const bool should_fetch = entity.IsMaskedEntity() &&
                            entity.IsServerInstance() &&
                            will_fill_sensitive_info;
  const bool should_reauth =
      will_fill_sensitive_info && prefs::IsAutofillAiReauthBeforeFillingEnabled(
                                      manager_->client().GetPrefs());

  callback = base::BindOnce(&AutofillAiAccessManager::MaybeUnmaskServerEntity,
                            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                            should_fetch);

  MaybeAuthenticate(std::move(entity), should_reauth, std::move(callback));
  return should_fetch || should_reauth;
}

void AutofillAiAccessManager::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (authenticator_ && is_authentication_in_progress_) {
    authenticator_->Cancel();
    authenticator_.reset();
  }
  is_authentication_in_progress_ = false;
}

void AutofillAiAccessManager::MaybeAuthenticate(
    EntityInstance entity,
    bool should_reauth,
    OnEntityInstanceFetchedCallback callback) {
  if (!should_reauth) {
    std::move(callback).Run(std::move(entity), /*reauth_attempted=*/false);
    return;
  }

  base::OnceCallback<void(bool)> on_auth_complete = base::BindOnce(
      [](EntityInstance entity, OnEntityInstanceFetchedCallback callback,
         bool auth_succeeded) {
        if (auth_succeeded) {
          std::move(callback).Run(std::move(entity), /*reauth_attempted=*/true);
        } else {
          // TODO(b/489690454): Emit this metric for Wallet entities.
          if (entity.record_type() ==
              EntityInstance::RecordType::kPersonalContext) {
            LogUnmaskResult(entity.record_type(),
                            AutofillAiUnmaskResult::kReauthFailed);
          }
          std::move(callback).Run(
              base::unexpected(FailureReason::kReauthFailed),
              /*reauth_attempted=*/true);
        }
      },
      std::move(entity), std::move(callback));

  Authenticate(manager_->client().GetLastCommittedPrimaryMainFrameOrigin(),
               std::move(on_auth_complete));
}

void AutofillAiAccessManager::Authenticate(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  if (!authenticator_) {
    authenticator_ =
        manager_->client().GetDeviceAuthenticator("Autofill.Ai.ReauthToFill");
  }
  if (!authenticator_ ||
      !authenticator_->CanAuthenticateWithBiometricOrScreenLock()) {
    // If the device is not capable of reauth or not set up, we assume success
    // to avoid blocking the user. Reauth is a best-effort security measure.
    std::move(callback).Run(/*auth_succeeded=*/true);
    return;
  }

  is_authentication_in_progress_ = true;
  authenticator_->AuthenticateWithMessage(
      GetAuthenticationMessage(origin),
      base::BindOnce(
          [](base::WeakPtr<AutofillAiAccessManager> self,
             base::OnceCallback<void(bool)> callback, bool auth_succeeded) {
            // Passing a weak pointer to `AutofillAiAccessManager` is needed
            // to ensure that the authentication is considered a failure if
            // `Reset()` was called during the authentication.
            if (!self) {
              std::move(callback).Run(/*auth_succeeded=*/false);
              return;
            }
            self->is_authentication_in_progress_ = false;
            self->authenticator_.reset();
            std::move(callback).Run(auth_succeeded);
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AutofillAiAccessManager::MaybeUnmaskServerEntity(
    OnEntityInstanceFetchedCallback callback,
    bool should_fetch,
    base::expected<EntityInstance, FailureReason> result,
    bool reauth_attempted) {
  if (!should_fetch || !result.has_value()) {
    std::move(callback).Run(std::move(result), reauth_attempted);
    return;
  }

  EntityInstance entity = std::move(result).value();
  CHECK(entity.IsServerInstance());

  auto on_unmasked_entity_fetched = base::BindOnce(
      [](base::WeakPtr<AutofillAiAccessManager> self,
         OnEntityInstanceFetchedCallback callback, bool reauth_attempted,
         std::optional<EntityInstance> fetched_entity) {
        // Passing a weak pointer to `AutofillAiAccessManager` is
        // needed to ensure that the callback is cancelled if
        // `Reset()` was called during the fetching.
        if (!self) {
          return;
        }
        if (fetched_entity) {
          std::move(callback).Run(std::move(*fetched_entity), reauth_attempted);
        } else {
          std::move(callback).Run(base::unexpected(FailureReason::kFetchFailed),
                                  reauth_attempted);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), reauth_attempted);

  switch (entity.record_type()) {
    case EntityInstance::RecordType::kServerWallet: {
      if (!manager_->client().GetWalletPassAccessManager()) {
        std::move(on_unmasked_entity_fetched).Run(std::nullopt);
        return;
      }
      manager_->client()
          .GetWalletPassAccessManager()
          ->GetUnmaskedWalletEntityInstance(
              entity.guid(), std::move(on_unmasked_entity_fetched));
      break;
    }
    case EntityInstance::RecordType::kPersonalContext: {
      if (!manager_->client().GetAutofillAiPersonalContextAccessManager()) {
        std::move(on_unmasked_entity_fetched).Run(std::nullopt);
        return;
      }
      manager_->client()
          .GetAutofillAiPersonalContextAccessManager()
          ->GetUnmaskedSpiiEntity(entity.guid(),
                                  std::move(on_unmasked_entity_fetched));
      break;
    }
    case EntityInstance::RecordType::kLocal:
      NOTREACHED();
  }
}

}  // namespace autofill

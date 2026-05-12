// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace autofill {

namespace {

std::u16string GetAuthenticationMessage(const url::Origin& origin) {
  // Android is excluded here because the system biometric prompt does not
  // support a custom message.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_IOS)
  // TODO(crbug.com/492978632): Evaluate if the host should be accessed based
  // on the field origin instead of using the last committed main frame origin
  // and what should happen when `host` is empty.
  return l10n_util::GetStringFUTF16(IDS_AUTOFILL_AI_FILLING_REAUTH,
                                    base::UTF8ToUTF16(origin.host()));
#else
  return std::u16string();
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_IOS)
}

}  // namespace

AutofillAiAccessManager::AutofillAiAccessManager(
    BrowserAutofillManager* manager)
    : manager_(CHECK_DEREF(manager)) {
  authenticator_ =
      manager_->client().GetDeviceAuthenticator("Autofill.Ai.ReauthToFill");
}

AutofillAiAccessManager::~AutofillAiAccessManager() = default;

bool AutofillAiAccessManager::FetchEntityInstance(
    EntityInstance entity,
    bool will_fill_sensitive_info,
    OnEntityInstanceFetchedCallback callback) {
  // Invalidate any pending operations from prior flows, ensuring that only one
  // flow is active at a time.
  Reset();

  const bool should_fetch = entity.IsMaskedEntity() &&
                            entity.IsServerInstance() &&
                            will_fill_sensitive_info;
  const bool should_reauth =
      will_fill_sensitive_info && prefs::IsAutofillAiReauthBeforeFillingEnabled(
                                      manager_->client().GetPrefs());

  if (should_fetch) {
    callback =
        base::BindOnce(&AutofillAiAccessManager::MaybeUnmaskServerEntity,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  }

  MaybeAuthenticate(std::move(entity), should_reauth, std::move(callback));
  return should_fetch || should_reauth;
}

void AutofillAiAccessManager::Reset() {
  if (authenticator_) {
    authenticator_->Cancel();
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
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
  if (!authenticator_ ||
      !authenticator_->CanAuthenticateWithBiometricOrScreenLock()) {
    // If the device is not capable of reauth or not set up, we assume success
    // to avoid blocking the user. Reauth is a best-effort security measure.
    std::move(callback).Run(/*auth_succeeded=*/true);
    return;
  }

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
            std::move(callback).Run(auth_succeeded);
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AutofillAiAccessManager::MaybeUnmaskServerEntity(
    OnEntityInstanceFetchedCallback callback,
    base::expected<EntityInstance, FailureReason> result,
    bool reauth_attempted) {
  if (!result.has_value()) {
    std::move(callback).Run(std::move(result), reauth_attempted);
    return;
  }

  EntityInstance entity = std::move(result).value();

  if (!manager_->client().GetWalletPassAccessManager()) {
    std::move(callback).Run(base::unexpected(FailureReason::kFetchFailed),
                            reauth_attempted);
    return;
  }

  manager_->client()
      .GetWalletPassAccessManager()
      ->GetUnmaskedWalletEntityInstance(
          entity.guid(),
          base::BindOnce(
              [](base::WeakPtr<AutofillAiAccessManager> self,
                 OnEntityInstanceFetchedCallback callback,
                 bool reauth_attempted,
                 std::optional<EntityInstance> fetched_entity) {
                // Passing a weak pointer to `AutofillAiAccessManager` is needed
                // to ensure that the callback is cancelled if `Reset()` was
                // called during the fetching.
                if (!self) {
                  return;
                }
                if (fetched_entity) {
                  std::move(callback).Run(std::move(*fetched_entity),
                                          reauth_attempted);
                } else {
                  std::move(callback).Run(
                      base::unexpected(FailureReason::kFetchFailed),
                      reauth_attempted);
                }
              },
              weak_ptr_factory_.GetWeakPtr(), std::move(callback),
              reauth_attempted));
}

}  // namespace autofill

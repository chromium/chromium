// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_SERVICE_H_
#define CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_SERVICE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_request.h"

namespace syncer {
class SyncService;
}

namespace chromeos {

// PasskeyService provides information about the state of the passkey security
// domain for the primary account of a given profile.
class PasskeyService : public KeyedService,
                       public signin::IdentityManager::Observer,
                       public trusted_vault::TrustedVaultClient::Observer {
 public:
  enum class AccountState {
    // No passkeys security domain exists. The device has a local LSKF that
    // can be uploaded for recovery.
    kEmpty,
    // No passkeys security domain exists, and the device does not have a local
    // LSKF that can be uploaded for recovery.
    kEmptyAndNoLocalRecoveryFactors,
    // The security domain exists. The domain secret is not known locally and
    // must be recovered by the user first.
    kNeedsRecovery,
    // The security domain exists but is irrecoverable.
    kIrrecoverable,
    // The security domain secret is available locally.
    kReady,
    // The account state could not be determined.
    kError,
  };

  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer when the passkeys domain secret was retrieved.
    virtual void OnHavePasskeysDomainSecret() = 0;
  };

  using AccountStateCallback = base::OnceCallback<void(AccountState)>;

  // All arguments must be non-null. `identity_manager`, `sync_service`,
  // and `trusted_vault_client` must outlive `this`.
  PasskeyService(signin::IdentityManager* identity_manager,
                 syncer::SyncService* sync_service,
                 trusted_vault::TrustedVaultClient* trusted_vault_client,
                 std::unique_ptr<trusted_vault::TrustedVaultConnection>
                     trusted_vault_connection);
  ~PasskeyService() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether GPM passkeys are enabled for the current primary account.
  bool GpmPasskeysAvailable();

  // Determines the current `AccountState` for the current primary account.
  void FetchAccountState(AccountStateCallback callback);

  // Returns the current security domain secret for the passkeys security domain
  // if available locally. FetchAccountState() should be called to ensure the
  // domain secret is available locally first.
  std::optional<std::vector<uint8_t>> GetCachedSecurityDomainSecret();

 private:
  void UpdatePrimaryAccount();
  void MaybeFetchTrustedVaultKeys();
  void OnFetchTrustedVaultKeys(
      const std::vector<std::vector<uint8_t>>& trusted_vault_keys);
  void MaybeDownloadAccountState();
  void OnDownloadAccountState(
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
          result);
  void RunPendingAccountStateCallbacks(AccountState state);

  // IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // TrustedVaultClient::Observer:
  void OnTrustedVaultKeysChanged() override;
  void OnTrustedVaultRecoverabilityChanged() override;

  std::optional<CoreAccountInfo> primary_account_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<syncer::SyncService> sync_service_;
  raw_ptr<trusted_vault::TrustedVaultClient> trusted_vault_client_;
  std::unique_ptr<trusted_vault::TrustedVaultConnection>
      trusted_vault_connection_;

  base::ObserverList<Observer> observers_;

  bool pending_fetch_trusted_vault_keys_ = false;
  std::vector<std::vector<uint8_t>> trusted_vault_keys_;
  std::vector<AccountStateCallback> pending_account_state_callbacks_;

  std::unique_ptr<trusted_vault::TrustedVaultConnection::Request>
      download_account_state_request_;

  base::WeakPtrFactory<PasskeyService> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_SERVICE_H_

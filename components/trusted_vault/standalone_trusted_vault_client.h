// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_CLIENT_H_
#define COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_CLIENT_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_histograms.h"

struct CoreAccountInfo;
class GaiaId;

namespace trusted_vault {

enum class SecurityDomainId;
class StandaloneTrustedVaultFrontend;

// Standalone, file-based implementation of TrustedVaultClient that stores the
// keys in a local file, containing a serialized protocol buffer.
//
// Reading of the file is done lazily.
class StandaloneTrustedVaultClient : public TrustedVaultClient {
 public:
  StandaloneTrustedVaultClient(
      SecurityDomainId security_domain,
      scoped_refptr<StandaloneTrustedVaultFrontend> frontend);

  StandaloneTrustedVaultClient(const StandaloneTrustedVaultClient& other) =
      delete;
  StandaloneTrustedVaultClient& operator=(
      const StandaloneTrustedVaultClient& other) = delete;
  ~StandaloneTrustedVaultClient() override;

  // TrustedVaultClient implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void FetchKeys(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb)
      override;
  void StoreKeys(
      const GaiaId& gaia_id,
      const std::vector<std::vector<uint8_t>>& keys,
      int last_key_version,
      std::optional<TrustedVaultUserActionTriggerForUMA> trigger) override;
  void MarkLocalKeysAsStale(const CoreAccountInfo& account_info,
                            base::OnceCallback<void(bool)> cb) override;
  void GetIsRecoverabilityDegraded(const CoreAccountInfo& account_info,
                                   base::OnceCallback<void(bool)> cb) override;
  void AddTrustedRecoveryMethod(const GaiaId& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint,
                                base::OnceCallback<void(bool)> cb) override;
  void ClearLocalDataForAccount(const CoreAccountInfo& account_info) override;

  // Notifications routed from StandaloneTrustedVaultFrontend on the UI thread.
  void NotifyTrustedVaultKeysChanged(
      std::optional<TrustedVaultUserActionTriggerForUMA> trigger);
  void NotifyRecoverabilityDegradedChanged();

  // Runs |cb| when the backend becomes idle.
  void WaitForIdleForTesting(base::OnceClosure cb) const;
  // TODO(crbug.com/40178774): Remove this API and rely exclusively on
  // FakeSecurityDomainsServer.
  void GetLastAddedRecoveryMethodPublicKeyForTesting(
      base::OnceCallback<void(const std::vector<uint8_t>&)> callback);
  void GetLastKeyVersionForTesting(
      const GaiaId& gaia_id,
      base::OnceCallback<void(int last_key_version)> callback);

 private:
  const SecurityDomainId security_domain_;
  const scoped_refptr<StandaloneTrustedVaultFrontend> frontend_;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_CLIENT_H_

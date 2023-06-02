// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CLIENT_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"

struct CoreAccountInfo;

namespace trusted_vault {

// Interface that allows platform-specific logic related to accessing locally
// available trusted vault encryption keys.
class TrustedVaultClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    // Invoked when the keys inside the vault have changed.
    virtual void OnTrustedVaultKeysChanged() = 0;

    // Invoked when the recoverability of the keys has changed.
    virtual void OnTrustedVaultRecoverabilityChanged() = 0;
  };

  TrustedVaultClient() = default;

  TrustedVaultClient(const TrustedVaultClient&) = delete;
  TrustedVaultClient& operator=(const TrustedVaultClient&) = delete;

  virtual ~TrustedVaultClient() = default;

  // Adds/removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Attempts to fetch decryption keys, required by sync to resume.
  // Implementations are expected to NOT prompt the user for actions. |cb| is
  // called on completion with known keys or an empty list if none known.
  virtual void FetchKeys(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
          cb) = 0;

  // Invoked when the result of FetchKeys() contains keys that cannot decrypt
  // the pending cryptographer (Nigori) keys, which should only be possible if
  // the provided keys are not up-to-date. |cb| is run upon completion and
  // returns false if the call did not make any difference (e.g. the operation
  // is unsupported) or true if some change may have occurred (which indicates a
  // second FetchKeys() attempt is worth). During the execution, before |cb| is
  // invoked, the behavior is unspecified if FetchKeys() is invoked, that is,
  // FetchKeys() may or may not treat existing keys as stale (only guaranteed
  // upon completion of MarkLocalKeysAsStale()).
  virtual void MarkLocalKeysAsStale(const CoreAccountInfo& account_info,
                                    base::OnceCallback<void(bool)> cb) = 0;

  // Allows implementations to store encryption keys fetched by other means such
  // as Web interactions. Implementations are free to completely ignore these
  // keys, so callers may not assume that later calls to FetchKeys() would
  // necessarily return the keys passed here.
  virtual void StoreKeys(const std::string& gaia_id,
                         const std::vector<std::vector<uint8_t>>& keys,
                         int last_key_version) = 0;

  // Returns whether recoverability of the keys is degraded and user action is
  // required to add a new method. This may be called frequently and
  // implementations are responsible for implementing caching and possibly
  // throttling.
  virtual void GetIsRecoverabilityDegraded(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(bool)> cb) = 0;

  // Registers a new trusted recovery method that can be used to retrieve keys,
  // usually for the purpose of resolving a recoverability-degraded case
  // surfaced by GetIsRecoverabilityDegraded(). |method_type_hint| is an opaque
  // value provided server-side that may be used for related future
  // interactions with the server.
  virtual void AddTrustedRecoveryMethod(const std::string& gaia_id,
                                        const std::vector<uint8_t>& public_key,
                                        int method_type_hint,
                                        base::OnceClosure cb) = 0;

  // Clears all data associated with |account_info|. Doesn't remove account from
  // storage.
  virtual void ClearLocalDataForAccount(
      const CoreAccountInfo& account_info) = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CLIENT_H_

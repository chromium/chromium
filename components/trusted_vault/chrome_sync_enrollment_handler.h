// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_CHROME_SYNC_ENROLLMENT_HANDLER_H_
#define COMPONENTS_TRUSTED_VAULT_CHROME_SYNC_ENROLLMENT_HANDLER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/trusted_vault_connection.h"

namespace trusted_vault {

class KeyStorage;

// Helper class used by `StandaloneTrustedVaultBackend` to handle enrollment
// into the `kChromeSync` security domain for a given `primary_account`.
class ChromeSyncEnrollmentHandler {
 public:
  // Invoked with true if `primary_account_` is eligible for non-interactive
  // enrollment into the `kChromeSync` security domain, or false otherwise.
  using CanEnrollCallback = base::OnceCallback<void(bool)>;

  // `storage` and `connection` must not be null and must outlive this object.
  ChromeSyncEnrollmentHandler(CoreAccountInfo primary_account,
                              KeyStorage* storage,
                              TrustedVaultConnection* connection);
  ChromeSyncEnrollmentHandler(const ChromeSyncEnrollmentHandler&) = delete;
  ChromeSyncEnrollmentHandler& operator=(const ChromeSyncEnrollmentHandler&) =
      delete;
  ~ChromeSyncEnrollmentHandler();

  // Asynchronously checks whether `primary_account_` can be enrolled in the
  // `kChromeSync` security domain without user interaction (i.e. the security
  // domain is in the pre-enrolled state and a valid Gaia password public key
  // is available on the server).
  void CanEnroll(CanEnrollCallback callback);

 private:
  // Encapsulates the state for an in-flight `CanEnroll()` request and allows
  // concurrent requests to be coalesced. Destroying this cancels the ongoing
  // request via RAII without invoking `callbacks`.
  struct OngoingCanEnroll {
    OngoingCanEnroll();
    OngoingCanEnroll(const OngoingCanEnroll&) = delete;
    OngoingCanEnroll& operator=(const OngoingCanEnroll&) = delete;
    OngoingCanEnroll(OngoingCanEnroll&&) noexcept;
    OngoingCanEnroll& operator=(OngoingCanEnroll&&) noexcept;
    ~OngoingCanEnroll();

    std::unique_ptr<TrustedVaultConnection::Request> request;
    std::vector<CanEnrollCallback> callbacks;
  };

  bool SatisfiesLocalPrerequisites() const;

  void OnGaiaPasswordPublicKeyDownloadedForCanEnroll(
      TrustedVaultDownloadPasswordPublicKeyStatus status,
      const std::vector<uint8_t>& public_key);

  const CoreAccountInfo primary_account_;
  const raw_ref<KeyStorage> storage_;
  const raw_ref<TrustedVaultConnection> connection_;

  std::optional<OngoingCanEnroll> ongoing_can_enroll_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_CHROME_SYNC_ENROLLMENT_HANDLER_H_

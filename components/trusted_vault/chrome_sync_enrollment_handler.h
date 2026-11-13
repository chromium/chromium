// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_CHROME_SYNC_ENROLLMENT_HANDLER_H_
#define COMPONENTS_TRUSTED_VAULT_CHROME_SYNC_ENROLLMENT_HANDLER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/enrollment_handler.h"
#include "components/trusted_vault/trusted_vault_connection.h"

namespace trusted_vault {

class KeyStorage;
enum class TrustedVaultCanEnrollOutcomeForUMA;
enum class TrustedVaultEnrollOutcomeForUMA;

// Handles enrollment into the `kChromeSync` security domain using the user's
// Gaia password public key as recovery factor.
class ChromeSyncEnrollmentHandler : public EnrollmentHandler {
 public:
  // `storage` and `connection` must not be null and must outlive this object.
  ChromeSyncEnrollmentHandler(CoreAccountInfo primary_account,
                              KeyStorage* storage,
                              TrustedVaultConnection* connection);
  ChromeSyncEnrollmentHandler(const ChromeSyncEnrollmentHandler&) = delete;
  ChromeSyncEnrollmentHandler& operator=(const ChromeSyncEnrollmentHandler&) =
      delete;
  ~ChromeSyncEnrollmentHandler() override;

  // EnrollmentHandler:
  void CanEnroll(CanEnrollCallback callback) override;
  void Enroll(EnrollCallback callback) override;

 private:
  // Represents why local key storage state does not allow non-interactive
  // enrollment into the `kChromeSync` security domain.
  enum class LocalIneligibility {
    kAlreadyEnrolled,
    kNoLocalKeys,
  };

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

  // Encapsulates the state for an in-flight `Enroll()` operation. Destroying
  // this cancels any active network request via RAII without invoking
  // `callback`.
  struct OngoingEnroll {
    OngoingEnroll();
    OngoingEnroll(const OngoingEnroll&) = delete;
    OngoingEnroll& operator=(const OngoingEnroll&) = delete;
    OngoingEnroll(OngoingEnroll&&) noexcept;
    OngoingEnroll& operator=(OngoingEnroll&&) noexcept;
    ~OngoingEnroll();

    EnrollCallback callback;
    std::unique_ptr<TrustedVaultConnection::Request> request;
    bool has_retried_key_rotation = false;
  };

  static TrustedVaultCanEnrollOutcomeForUMA
  GetCanEnrollOutcomeForUMAFromLocalIneligibility(
      LocalIneligibility local_ineligibility);
  static TrustedVaultEnrollOutcomeForUMA
  GetEnrollOutcomeForUMAFromLocalIneligibility(
      LocalIneligibility local_ineligibility);

  // Returns why local key storage does not allow enrollment, or `std::nullopt`
  // if local prerequisites are met.
  std::optional<LocalIneligibility> GetLocalIneligibility() const;

  void OnGaiaPasswordPublicKeyDownloadedForCanEnroll(
      TrustedVaultDownloadPasswordPublicKeyStatus status,
      const std::vector<uint8_t>& public_key);
  void FulfillOngoingCanEnrollWithFailure(
      TrustedVaultCanEnrollOutcomeForUMA outcome);

  void OnGaiaPasswordPublicKeyDownloadedForEnroll(
      TrustedVaultDownloadPasswordPublicKeyStatus status,
      const std::vector<uint8_t>& public_key);
  void OnGaiaPasswordRegisteredForEnroll(TrustedVaultRegistrationStatus status,
                                         int unused_epoch);
  void RotateSharedKeyForEnroll(const std::vector<uint8_t>& new_shared_key);
  void OnKeyRotatedForEnroll(const std::vector<uint8_t>& new_shared_key,
                             TrustedVaultKeyRotationStatus status,
                             int new_key_version);
  void FulfillOngoingEnrollWithFailure(TrustedVaultEnrollOutcomeForUMA outcome);

  const CoreAccountInfo primary_account_;
  const raw_ref<KeyStorage> storage_;
  const raw_ref<TrustedVaultConnection> connection_;

  std::optional<OngoingCanEnroll> ongoing_can_enroll_;
  std::optional<OngoingEnroll> ongoing_enroll_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_CHROME_SYNC_ENROLLMENT_HANDLER_H_

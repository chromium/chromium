// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_ENROLLMENT_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_ENROLLMENT_MANAGER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_manager.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager.h"
#include "chromeos/ash/services/device_sync/cryptauth_scheduler.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

namespace device_sync {

class CryptAuthClientFactory;
class CryptAuthKeyRegistry;
class CryptAuthV2Enroller;

// Implementation of CryptAuthEnrollmentManager for CryptAuth v2 Enrollment.
//
// This implementation considers three sources of enrollment requests:
//  1) The scheduler requests periodic enrollments and handles any failed
//     attempts.
//  2) The enrollment manager listens to the GCM manager for re-enrollment
//     requests.
//  3) The ForceEnrollmentNow() method allows for immediate requests.
//  4) On start-up, if the client app metadata has changed since the last
//     enrollment, a re-enrollment is scheduled.
//
// All flavors of enrollment attempts are guarded by timeouts. For example, an
// enrollment attempt triggered by ForceEnrollmentNow() will always
// conclude--successfully or not--in an allotted period of time.
//
// The v2 Enrollment infrastructure stores keys in a CryptAuthKeyRegistry. In
// contrast, the v1 enrollment manager implementation directly controls a set of
// prefs that store the user key pair. However, on construction of
// CryptAuthV2EnrollmentManagerImpl, any existing v1 user key pair is added to
// the v2 key registry to ensure consistency across the v1 to v2 Enrollment
// migration. The converse is not true. The v1 prefs will never be modified by
// v2 Enrollment.
class CryptAuthV2EnrollmentManagerImpl
    : public CryptAuthEnrollmentManager,
      public CryptAuthScheduler::EnrollmentDelegate,
      public CryptAuthGCMManager::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthEnrollmentManager> Create(
        const cryptauthv2::ClientAppMetadata& client_app_metadata,
        CryptAuthKeyRegistry* key_registry,
        CryptAuthClientFactory* client_factory,
        CryptAuthGCMManager* gcm_manager,
        CryptAuthScheduler* scheduler,
        PrefService* pref_service,
        base::Clock* clock = base::DefaultClock::GetInstance());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthEnrollmentManager> CreateInstance(
        const cryptauthv2::ClientAppMetadata& client_app_metadata,
        CryptAuthKeyRegistry* key_registry,
        CryptAuthClientFactory* client_factory,
        CryptAuthGCMManager* gcm_manager,
        CryptAuthScheduler* scheduler,
        PrefService* pref_service,
        base::Clock* clock) = 0;

   private:
    static Factory* test_factory_;
  };

  // Note: This should never be called with
  // CryptAuthEnrollmentManagerImpl::RegisterPrefs().
  static void RegisterPrefs(PrefRegistrySimple* registry);

  CryptAuthV2EnrollmentManagerImpl(const CryptAuthV2EnrollmentManagerImpl&) =
      delete;
  CryptAuthV2EnrollmentManagerImpl& operator=(
      const CryptAuthV2EnrollmentManagerImpl&) = delete;

  ~CryptAuthV2EnrollmentManagerImpl() override;

 protected:
  CryptAuthV2EnrollmentManagerImpl(
      const cryptauthv2::ClientAppMetadata& client_app_metadata,
      CryptAuthKeyRegistry* key_registry,
      CryptAuthClientFactory* client_factory,
      CryptAuthGCMManager* gcm_manager,
      CryptAuthScheduler* scheduler,
      PrefService* pref_service,
      base::Clock* clock);

 private:
  // CryptAuthEnrollmentManager:
  void Start() override;
  void ForceEnrollmentNow(
      cryptauth::InvocationReason invocation_reason,
      const std::optional<std::string>& session_id) override;
  bool IsEnrollmentValid() const override;
  base::Time GetLastEnrollmentTime() const override;
  base::TimeDelta GetTimeToNextAttempt() const override;
  bool IsEnrollmentInProgress() const override;
  bool IsRecoveringFromFailure() const override;
  std::string GetUserPublicKey() const override;
  std::string GetUserPrivateKey() const override;

  // CryptAuthScheduler::EnrollmentDelegate:
  void OnEnrollmentRequested(const cryptauthv2::ClientMetadata& client_metadata,
                             const std::optional<cryptauthv2::PolicyReference>&
                                 client_directive_policy_reference) override;

  // CryptAuthGCMManager::Observer:
  void OnReenrollMessage(
      const std::optional<std::string>& session_id,
      const std::optional<CryptAuthFeatureType>& feature_type) override;

  void Enroll();
  void OnEnrollmentFinished(const CryptAuthEnrollmentResult& enrollment_result);

  std::string GetClientAppMetadataHash() const;
  std::string GetV1UserPublicKey() const;
  std::string GetV1UserPrivateKey() const;

  // If a v1 user key-pair exists, add it to the registry as the active key in
  // the kUserKeyPair key bundle.
  void AddV1UserKeyPairToRegistryIfNecessary();

  cryptauthv2::ClientAppMetadata client_app_metadata_;
  raw_ptr<CryptAuthKeyRegistry> key_registry_;
  raw_ptr<CryptAuthClientFactory> client_factory_;
  raw_ptr<CryptAuthGCMManager> gcm_manager_;
  raw_ptr<CryptAuthScheduler> scheduler_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<base::Clock> clock_;

  bool initial_v1_and_v2_user_key_pairs_disagree_ = false;

  std::optional<cryptauthv2::ClientMetadata> current_client_metadata_;
  std::optional<cryptauthv2::PolicyReference>
      client_directive_policy_reference_;
  std::unique_ptr<CryptAuthV2Enroller> enroller_;

  // For weak pointers used in callbacks. These weak pointers are invalidated
  // when the current enrollment attempt finishes in order to cancel outstanding
  // callbacks.
  base::WeakPtrFactory<CryptAuthV2EnrollmentManagerImpl>
      callback_weak_ptr_factory_{this};

  // For sending a weak pointer to the scheduler, whose lifetime exceeds that of
  // CryptAuthV2EnrollmentManagerImpl.
  base::WeakPtrFactory<CryptAuthV2EnrollmentManagerImpl>
      scheduler_weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_ENROLLMENT_MANAGER_IMPL_H_

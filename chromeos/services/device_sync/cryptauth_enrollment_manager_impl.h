// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_manager.h"
#include "chromeos/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/services/device_sync/cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/sync_scheduler.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}

namespace chromeos {

namespace multidevice {
class SecureMessageDelegate;
}  // namespace multidevice

namespace device_sync {

class CryptAuthEnroller;
class CryptAuthEnrollerFactory;

// Concrete CryptAuthEnrollmentManager implementation.
//
// This implementation considers three sources of enrollment requests:
//  1) A sync scheduler requests periodic enrollments and handles any failed
//     attempts.
//  2) The enrollment manager listens to the GCM manager for re-enrollment
//     requests.
//  3) The ForceEnrollmentNow() method allows for immediate requests.
//
// When an enrollment has been requested, this implementation generates a user
// key pair, if one doesn't already exists, and persists these keys as
// preferences. Thus, the user key pair should never rotate.
//
// This implementation also determines the times between enrollment attempts,
// which is roughly 30 days after a successful enrollments and 10 minutes after
// a failed enrollment attempt, exponentially increasing for consecutive
// failures. An enrollment is considered "invalid" after 45 days.
class CryptAuthEnrollmentManagerImpl : public CryptAuthEnrollmentManager,
                                       public SyncScheduler::Delegate,
                                       public CryptAuthGCMManager::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthEnrollmentManager> NewInstance(
        base::Clock* clock,
        std::unique_ptr<CryptAuthEnrollerFactory> enroller_factory,
        std::unique_ptr<multidevice::SecureMessageDelegate>
            secure_message_delegate,
        const cryptauth::GcmDeviceInfo& device_info,
        CryptAuthGCMManager* gcm_manager,
        PrefService* pref_service);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthEnrollmentManager> BuildInstance(
        base::Clock* clock,
        std::unique_ptr<CryptAuthEnrollerFactory> enroller_factory,
        std::unique_ptr<multidevice::SecureMessageDelegate>
            secure_message_delegate,
        const cryptauth::GcmDeviceInfo& device_info,
        CryptAuthGCMManager* gcm_manager,
        PrefService* pref_service);

   private:
    static Factory* factory_instance_;
  };

  // Registers the prefs used by this class to the given |pref_service|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~CryptAuthEnrollmentManagerImpl() override;

  // CryptAuthEnrollmentManager:
  void Start() override;
  void ForceEnrollmentNow(
      cryptauth::InvocationReason invocation_reason,
      const base::Optional<std::string>& session_id) override;
  bool IsEnrollmentValid() const override;
  base::Time GetLastEnrollmentTime() const override;
  base::TimeDelta GetTimeToNextAttempt() const override;
  bool IsEnrollmentInProgress() const override;
  bool IsRecoveringFromFailure() const override;
  std::string GetUserPublicKey() const override;
  std::string GetUserPrivateKey() const override;

 protected:
  // Creates the manager:
  // |clock|: Used to determine the time between sync attempts.
  // |enroller_factory|: Creates CryptAuthEnroller instances to perform each
  //                     enrollment attempt.
  // |secure_message_delegate|: Used to generate the user's keypair if it does
  //                            not exist.
  // |device_info|: Contains information about the local device that will be
  //                uploaded to CryptAuth with each enrollment request.
  // |gcm_manager|: Used to perform GCM registrations and also notifies when GCM
  //                push messages trigger re-enrollments.
  //                Not owned and must outlive this instance.
  // |pref_service|: Contains preferences across browser restarts, and should
  //                 have been registered through RegisterPrefs().
  CryptAuthEnrollmentManagerImpl(
      base::Clock* clock,
      std::unique_ptr<CryptAuthEnrollerFactory> enroller_factory,
      std::unique_ptr<multidevice::SecureMessageDelegate>
          secure_message_delegate,
      const cryptauth::GcmDeviceInfo& device_info,
      CryptAuthGCMManager* gcm_manager,
      PrefService* pref_service);

  void SetSyncSchedulerForTest(std::unique_ptr<SyncScheduler> sync_scheduler);

 private:
  // CryptAuthGCMManager::Observer:
  void OnGCMRegistrationResult(bool success) override;
  void OnReenrollMessage(
      const base::Optional<std::string>& session_id,
      const base::Optional<CryptAuthFeatureType>& feature_type) override;

  // Callback when a new keypair is generated.
  void OnKeyPairGenerated(const std::string& public_key,
                          const std::string& private_key);

  // SyncScheduler::Delegate:
  void OnSyncRequested(
      std::unique_ptr<SyncScheduler::SyncRequest> sync_request) override;

  // Starts a CryptAuth enrollment attempt, generating a new keypair if one is
  // not already stored in the user prefs.
  void DoCryptAuthEnrollment();

  // Starts a CryptAuth enrollment attempt, after a key-pair is stored in the
  // user prefs.
  void DoCryptAuthEnrollmentWithKeys();

  // Callback when |cryptauth_enroller_| completes.
  void OnEnrollmentFinished(bool success);

  // Used to determine the time.
  base::Clock* clock_;

  // Creates CryptAuthEnroller instances for each enrollment attempt.
  std::unique_ptr<CryptAuthEnrollerFactory> enroller_factory_;

  // The SecureMessageDelegate used to generate the user's keypair if it does
  // not already exist.
  std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate_;

  // The local device information to upload to CryptAuth.
  const cryptauth::GcmDeviceInfo device_info_;

  //  Used to perform GCM registrations and also notifies when GCM push messages
  //  trigger re-enrollments. Not owned and must outlive this instance.
  CryptAuthGCMManager* gcm_manager_;

  // Contains perferences that outlive the lifetime of this object and across
  // process restarts.
  // Not owned and must outlive this instance.
  PrefService* pref_service_;

  // Schedules the time between enrollment attempts.
  std::unique_ptr<SyncScheduler> scheduler_;

  // Contains the SyncRequest that |scheduler_| requests when an enrollment
  // attempt is made.
  std::unique_ptr<SyncScheduler::SyncRequest> sync_request_;

  // The CryptAuthEnroller instance for the current enrollment attempt. A new
  // instance will be created for each individual attempt.
  std::unique_ptr<CryptAuthEnroller> cryptauth_enroller_;

  base::WeakPtrFactory<CryptAuthEnrollmentManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CryptAuthEnrollmentManagerImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_MANAGER_IMPL_H_

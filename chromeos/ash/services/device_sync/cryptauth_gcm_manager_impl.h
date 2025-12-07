// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_GCM_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_GCM_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/instance_id/instance_id.h"

class PrefService;

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace instance_id {
class InstanceIDDriver;
}  // namespace instance_id

namespace ash {

namespace device_sync {

// Implementation of CryptAuthGCMManager.
class CryptAuthGCMManagerImpl : public CryptAuthGCMManager,
                                public gcm::GCMAppHandler {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthGCMManager> Create(
        gcm::GCMDriver* gcm_driver,
        instance_id::InstanceIDDriver* instance_id_driver,
        PrefService* pref_service);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthGCMManager> CreateInstance(
        gcm::GCMDriver* gcm_driver,
        instance_id::InstanceIDDriver* instance_id_driver,
        PrefService* pref_service) = 0;

   private:
    static Factory* factory_instance_;
  };

  CryptAuthGCMManagerImpl(const CryptAuthGCMManagerImpl&) = delete;
  CryptAuthGCMManagerImpl& operator=(const CryptAuthGCMManagerImpl&) = delete;

  ~CryptAuthGCMManagerImpl() override;

  // CryptAuthGCMManager:
  void StartListening() override;
  bool IsListening() override;
  void RegisterWithGCM() override;
  std::string GetRegistrationId() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 protected:
  // Creates the manager:
  // |instance_id_driver|: Handles the actual GCM communications. The driver is
  // not
  //     owned and must outlive this instance.
  // |pref_service|: Contains preferences across browser restarts, and should
  //     have been registered through RegisterPrefs(). The service is not owned
  //     and must outlive this instance.
  CryptAuthGCMManagerImpl(gcm::GCMDriver* gcm_driver,
                          instance_id::InstanceIDDriver* instance_id_driver,
                          PrefService* pref_service);

 private:
  friend class DeviceSyncCryptAuthGCMManagerImplTest;

  // GCMAppHandler:
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(const std::string& app_id,
                   const gcm::GCMClient::SendErrorDetails& details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

  // Called when GCM registration completes.
  void OnRegistrationCompleted(const std::string& registration_id,

                               instance_id::InstanceID::Result result);

  raw_ptr<gcm::GCMDriver> gcm_driver_;

  // Handles the communications with GCM. Not owned.
  raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_;

  // Manages preferences across process restarts. Not owned.
  raw_ptr<PrefService> pref_service_;

  // Whether a GCM registration is currently being processed.
  bool registration_in_progress_;

  // The time GCM registration starts. Used for execution time metrics.
  base::TimeTicks gcm_registration_start_timestamp_;

  // List of observers.
  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<CryptAuthGCMManagerImpl> weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_GCM_MANAGER_IMPL_H_

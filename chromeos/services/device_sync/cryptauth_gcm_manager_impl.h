// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_GCM_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_GCM_MANAGER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/services/device_sync/cryptauth_gcm_manager.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client.h"

class PrefService;

namespace gcm {
class GCMDriver;
}

namespace chromeos {

namespace device_sync {

// Implementation of CryptAuthGCMManager.
class CryptAuthGCMManagerImpl : public CryptAuthGCMManager,
                                public gcm::GCMAppHandler {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthGCMManager> NewInstance(
        gcm::GCMDriver* gcm_driver,
        PrefService* pref_service);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthGCMManager> BuildInstance(
        gcm::GCMDriver* gcm_driver,
        PrefService* pref_service);

   private:
    static Factory* factory_instance_;
  };

  ~CryptAuthGCMManagerImpl() override;

  // CryptAuthGCMManager:
  void StartListening() override;
  void RegisterWithGCM() override;
  std::string GetRegistrationId() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 protected:
  // Creates the manager:
  // |gcm_driver|: Handles the actual GCM communications. The driver is not
  //     owned and must outlive this instance.
  // |pref_service|: Contains preferences across browser restarts, and should
  //     have been registered through RegisterPrefs(). The service is not owned
  //     and must outlive this instance.
  CryptAuthGCMManagerImpl(gcm::GCMDriver* gcm_driver,
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
                               gcm::GCMClient::Result result);

  // Handles the communications with GCM. Not owned.
  gcm::GCMDriver* gcm_driver_;

  // Manages preferences across process restarts. Not owned.
  PrefService* pref_service_;

  // Whether a GCM registration is currently being processed.
  bool registration_in_progress_;

  // List of observers.
  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<CryptAuthGCMManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CryptAuthGCMManagerImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_GCM_MANAGER_IMPL_H_

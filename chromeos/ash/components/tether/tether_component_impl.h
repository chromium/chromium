// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_COMPONENT_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_COMPONENT_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/tether/tether_component.h"
#include "device/bluetooth/bluetooth_adapter.h"

class PrefService;

namespace session_manager {
class SessionManager;
}  // namespace session_manager

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

namespace device_sync {
class DeviceSyncClient;
}

namespace secure_channel {
class SecureChannelClient;
}

class NetworkConnect;

namespace tether {

class AsynchronousShutdownObjectContainer;
class CrashRecoveryManager;
class GmsCoreNotificationsStateTrackerImpl;
class NotificationPresenter;
class SynchronousShutdownObjectContainer;
class TetherHostFetcher;

// Initializes the Tether component.
class TetherComponentImpl : public TetherComponent {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  class Factory {
   public:
    static std::unique_ptr<TetherComponent> Create(
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client,
        TetherHostFetcher* tether_host_fetcher,
        NotificationPresenter* notification_presenter,
        GmsCoreNotificationsStateTrackerImpl*
            gms_core_notifications_state_tracker,
        PrefService* pref_service,
        NetworkHandler* network_handler,
        NetworkConnect* network_connect,
        scoped_refptr<device::BluetoothAdapter> adapter,
        session_manager::SessionManager* session_manager);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<TetherComponent> CreateInstance(
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client,
        TetherHostFetcher* tether_host_fetcher,
        NotificationPresenter* notification_presenter,
        GmsCoreNotificationsStateTrackerImpl*
            gms_core_notifications_state_tracker,
        PrefService* pref_service,
        NetworkHandler* network_handler,
        NetworkConnect* network_connect,
        scoped_refptr<device::BluetoothAdapter> adapter,
        session_manager::SessionManager* session_manager) = 0;

   private:
    static Factory* factory_instance_;
  };

  TetherComponentImpl(const TetherComponentImpl&) = delete;
  TetherComponentImpl& operator=(const TetherComponentImpl&) = delete;

  ~TetherComponentImpl() override;

  // TetherComponent:
  void RequestShutdown(const ShutdownReason& shutdown_reason) override;

 protected:
  TetherComponentImpl(
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      TetherHostFetcher* tether_host_fetcher,
      NotificationPresenter* notification_presenter,
      GmsCoreNotificationsStateTrackerImpl*
          gms_core_notifications_state_tracker,
      PrefService* pref_service,
      NetworkHandler* network_handler,
      NetworkConnect* network_connect,
      scoped_refptr<device::BluetoothAdapter> adapter,
      session_manager::SessionManager* session_manager);

 private:
  void OnPreCrashStateRestored();
  void InitiateShutdown();
  void OnShutdownComplete();

  std::unique_ptr<AsynchronousShutdownObjectContainer>
      asynchronous_shutdown_object_container_;
  std::unique_ptr<SynchronousShutdownObjectContainer>
      synchronous_shutdown_object_container_;
  std::unique_ptr<CrashRecoveryManager> crash_recovery_manager_;

  bool has_shutdown_been_requested_ = false;
  ShutdownReason shutdown_reason_;

  base::WeakPtrFactory<TetherComponentImpl> weak_ptr_factory_{this};
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_COMPONENT_IMPL_H_

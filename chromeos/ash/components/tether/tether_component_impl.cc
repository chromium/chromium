// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_component_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/tether/active_host.h"
#include "chromeos/ash/components/tether/asynchronous_shutdown_object_container_impl.h"
#include "chromeos/ash/components/tether/crash_recovery_manager_impl.h"
#include "chromeos/ash/components/tether/host_scan_scheduler.h"
#include "chromeos/ash/components/tether/persistent_host_scan_cache_impl.h"
#include "chromeos/ash/components/tether/synchronous_shutdown_object_container_impl.h"
#include "chromeos/ash/components/tether/tether_disconnector.h"
#include "chromeos/ash/components/tether/tether_host_response_recorder.h"
#include "chromeos/ash/components/tether/tether_session_completion_logger.h"
#include "chromeos/ash/components/tether/wifi_hotspot_disconnector_impl.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace ash {

namespace tether {

namespace {

void OnDisconnectErrorDuringShutdown(const std::string& error_name) {
  PA_LOG(WARNING) << "Error disconnecting from Tether network during shutdown; "
                  << "Error name: " << error_name;
}

TetherSessionCompletionLogger::SessionCompletionReason
GetSessionCompletionReasonFromShutdownReason(
    TetherComponent::ShutdownReason shutdown_reason) {
  switch (shutdown_reason) {
    case TetherComponent::ShutdownReason::OTHER:
      return TetherSessionCompletionLogger::SessionCompletionReason::OTHER;
    case TetherComponent::ShutdownReason::USER_LOGGED_OUT:
      return TetherSessionCompletionLogger::SessionCompletionReason::
          USER_LOGGED_OUT;
    case TetherComponent::ShutdownReason::USER_CLOSED_LID:
      return TetherSessionCompletionLogger::SessionCompletionReason::
          USER_CLOSED_LID;
    case TetherComponent::ShutdownReason::PREF_DISABLED:
      return TetherSessionCompletionLogger::SessionCompletionReason::
          PREF_DISABLED;
    case TetherComponent::ShutdownReason::BLUETOOTH_DISABLED:
      return TetherSessionCompletionLogger::SessionCompletionReason::
          BLUETOOTH_DISABLED;
    case TetherComponent::ShutdownReason::CELLULAR_DISABLED:
      return TetherSessionCompletionLogger::SessionCompletionReason::
          CELLULAR_DISABLED;
    case TetherComponent::ShutdownReason::MULTIDEVICE_HOST_UNVERIFIED:
      return TetherSessionCompletionLogger::SessionCompletionReason::
          MULTIDEVICE_HOST_UNVERIFIED;
    case TetherComponent::ShutdownReason::BETTER_TOGETHER_SUITE_DISABLED:
      return TetherSessionCompletionLogger::SessionCompletionReason::
          BETTER_TOGETHER_SUITE_DISABLED;
    default:
      break;
  }

  return TetherSessionCompletionLogger::SessionCompletionReason::OTHER;
}

}  // namespace

// static
TetherComponentImpl::Factory* TetherComponentImpl::Factory::factory_instance_ =
    nullptr;

// static
std::unique_ptr<TetherComponent> TetherComponentImpl::Factory::Create(
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    TetherHostFetcher* tether_host_fetcher,
    NotificationPresenter* notification_presenter,
    GmsCoreNotificationsStateTrackerImpl* gms_core_notifications_state_tracker,
    PrefService* pref_service,
    NetworkHandler* network_handler,
    NetworkConnect* network_connect,
    scoped_refptr<device::BluetoothAdapter> adapter,
    session_manager::SessionManager* session_manager) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(
        device_sync_client, secure_channel_client, tether_host_fetcher,
        notification_presenter, gms_core_notifications_state_tracker,
        pref_service, network_handler, network_connect, adapter,
        session_manager);
  }

  return base::WrapUnique(new TetherComponentImpl(
      device_sync_client, secure_channel_client, tether_host_fetcher,
      notification_presenter, gms_core_notifications_state_tracker,
      pref_service, network_handler, network_connect, adapter,
      session_manager));
}

// static
void TetherComponentImpl::Factory::SetFactoryForTesting(Factory* factory) {
  factory_instance_ = factory;
}

// static
void TetherComponentImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ActiveHost::RegisterPrefs(registry);
  PersistentHostScanCacheImpl::RegisterPrefs(registry);
  TetherHostResponseRecorder::RegisterPrefs(registry);
  WifiHotspotDisconnectorImpl::RegisterPrefs(registry);
}

TetherComponentImpl::TetherComponentImpl(
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    TetherHostFetcher* tether_host_fetcher,
    NotificationPresenter* notification_presenter,
    GmsCoreNotificationsStateTrackerImpl* gms_core_notifications_state_tracker,
    PrefService* pref_service,
    NetworkHandler* network_handler,
    NetworkConnect* network_connect,
    scoped_refptr<device::BluetoothAdapter> adapter,
    session_manager::SessionManager* session_manager)
    : asynchronous_shutdown_object_container_(
          AsynchronousShutdownObjectContainerImpl::Factory::Create(
              device_sync_client,
              secure_channel_client,
              tether_host_fetcher,
              network_handler->network_state_handler(),
              network_handler->managed_network_configuration_handler(),
              network_handler->network_connection_handler(),
              pref_service)),
      synchronous_shutdown_object_container_(
          SynchronousShutdownObjectContainerImpl::Factory::Create(
              asynchronous_shutdown_object_container_.get(),
              notification_presenter,
              gms_core_notifications_state_tracker,
              pref_service,
              network_handler,
              network_connect,
              session_manager,
              device_sync_client,
              secure_channel_client)),
      crash_recovery_manager_(CrashRecoveryManagerImpl::Factory::Create(
          network_handler->network_state_handler(),
          synchronous_shutdown_object_container_->active_host(),
          synchronous_shutdown_object_container_->host_scan_cache())) {
  crash_recovery_manager_->RestorePreCrashStateIfNecessary(
      base::BindOnce(&TetherComponentImpl::OnPreCrashStateRestored,
                     weak_ptr_factory_.GetWeakPtr()));
}

TetherComponentImpl::~TetherComponentImpl() = default;

void TetherComponentImpl::RequestShutdown(
    const ShutdownReason& shutdown_reason) {
  has_shutdown_been_requested_ = true;

  // If shutdown has already happened, there is nothing else to do.
  if (status() != TetherComponent::Status::ACTIVE) {
    return;
  }

  shutdown_reason_ = shutdown_reason;

  // If crash recovery has not yet completed, wait for it to complete before
  // continuing.
  if (crash_recovery_manager_) {
    return;
  }

  InitiateShutdown();
}

void TetherComponentImpl::OnPreCrashStateRestored() {
  // |crash_recovery_manager_| is no longer needed since it has completed.
  crash_recovery_manager_.reset();

  if (has_shutdown_been_requested_) {
    InitiateShutdown();
    return;
  }

  // Start a scan now that the Tether module has started up.
  synchronous_shutdown_object_container_->host_scan_scheduler()
      ->AttemptScanIfOffline();
}

void TetherComponentImpl::InitiateShutdown() {
  DCHECK(has_shutdown_been_requested_);
  DCHECK(!crash_recovery_manager_);
  DCHECK(status() == TetherComponent::Status::ACTIVE);

  ActiveHost* active_host =
      synchronous_shutdown_object_container_->active_host();
  TetherDisconnector* tether_disconnector =
      synchronous_shutdown_object_container_->tether_disconnector();

  // If there is an active connection, it needs to be disconnected before the
  // Tether component is shut down.
  if (active_host->GetActiveHostStatus() !=
      ActiveHost::ActiveHostStatus::DISCONNECTED) {
    PA_LOG(VERBOSE) << "There was an active connection during Tether shutdown. "
                    << "Initiating disconnection from device ID \""
                    << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                           active_host->GetActiveHostDeviceId())
                    << "\".";
    tether_disconnector->DisconnectFromNetwork(
        active_host->GetTetherNetworkGuid(), base::DoNothing(),
        base::BindOnce(&OnDisconnectErrorDuringShutdown),
        GetSessionCompletionReasonFromShutdownReason(shutdown_reason_));
  }

  TransitionToStatus(TetherComponent::Status::SHUTTING_DOWN);

  // Delete objects which can shutdown synchronously immediately.
  synchronous_shutdown_object_container_.reset();

  // Start the shutdown process for objects which shutdown asynchronously.
  asynchronous_shutdown_object_container_->Shutdown(
      base::BindOnce(&TetherComponentImpl::OnShutdownComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TetherComponentImpl::OnShutdownComplete() {
  DCHECK(status() == TetherComponent::Status::SHUTTING_DOWN);

  // Shutdown has completed. The asynchronous objects can now be deleted as
  // well.
  asynchronous_shutdown_object_container_.reset();

  TransitionToStatus(TetherComponent::Status::SHUT_DOWN);
}

}  // namespace tether

}  // namespace ash

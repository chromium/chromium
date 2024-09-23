// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_SERVICE_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_device_registration.h"
#include "components/sharing_message/sharing_message_sender.h"
#include "components/sharing_message/sharing_send_message_result.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/service/sync_service_observer.h"
#include "net/base/backoff_entry.h"

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace favicon_base {
struct FaviconRawBitmapResult;
}  // namespace favicon_base

namespace send_tab_to_self {
class SendTabToSelfEntry;
class SendTabToSelfModel;
}  // namespace send_tab_to_self

namespace syncer {
class SyncService;
}  // namespace syncer

class SharingDeviceSource;
class SharingFCMHandler;
class SharingHandlerRegistry;
class SharingMessageHandler;
class SharingSyncPreference;
class VapidKeyManager;
enum class SharingDeviceRegistrationResult;

// Class to manage lifecycle of sharing feature, and provide APIs to send
// sharing messages to other devices.
class SharingService : public KeyedService,
                       public syncer::SyncServiceObserver,
                       public send_tab_to_self::SendTabToSelfModelObserver {
 public:
  using SharingDeviceList = std::vector<SharingTargetDeviceInfo>;
  using NotificationActionCallback =
      base::RepeatingCallback<void(std::optional<int> button, bool closed)>;

  enum class State {
    // Device is unregistered with FCM and Sharing is unavailable.
    DISABLED,
    // Device registration is in progress.
    REGISTERING,
    // Device is fully registered with FCM and Sharing is available.
    ACTIVE,
    // Device un-registration is in progress.
    UNREGISTERING
  };

  SharingService(
      std::unique_ptr<SharingSyncPreference> sync_prefs,
      std::unique_ptr<VapidKeyManager> vapid_key_manager,
      std::unique_ptr<SharingDeviceRegistration> sharing_device_registration,
      std::unique_ptr<SharingMessageSender> message_sender,
      std::unique_ptr<SharingDeviceSource> device_source,
      std::unique_ptr<SharingHandlerRegistry> handler_registry,
      std::unique_ptr<SharingFCMHandler> fcm_handler,
      syncer::SyncService* sync_service,
      favicon::FaviconService* favicon_service,
      send_tab_to_self::SendTabToSelfModel* send_tab_model,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  SharingService(const SharingService&) = delete;
  SharingService& operator=(const SharingService&) = delete;
  ~SharingService() override;

  // Returns the device matching |guid| or nullopt if no match was found.
  virtual std::optional<SharingTargetDeviceInfo> GetDeviceByGuid(
      const std::string& guid) const;

  // Returns a list of devices that is available to receive messages.
  // All returned devices have the specified |required_feature|.
  virtual SharingDeviceList GetDeviceCandidates(
      sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const;

  // Sends a Sharing message to remote device.
  // |device|: The receiver device.
  // |response_timeout|: Maximum amount of time waiting for a response before
  // invoking |callback| with kAckTimeout.
  // |message|: Message to be sent.
  // |callback| will be invoked once a response has received from remote device,
  // or if operation has failed or timed out.
  // Returns a callback that cancels receiving of the response. Calling it will
  // run |callback| if it hasn't been executed yet, otherwise it will be a
  // no-op. Returns a null callback if the message is failed to be sent.
  virtual base::OnceClosure SendMessageToDevice(
      const SharingTargetDeviceInfo& device,
      base::TimeDelta response_timeout,
      components_sharing_message::SharingMessage message,
      SharingMessageSender::ResponseCallback callback);

  // Unencrypted message counterpart to the above function.
  virtual base::OnceClosure SendUnencryptedMessageToDevice(
      const SharingTargetDeviceInfo& device,
      sync_pb::UnencryptedSharingMessage message,
      SharingMessageSender::ResponseCallback callback);

  // Register SharingMessageHandler for |payload_cases|.
  void RegisterSharingHandler(
      std::unique_ptr<SharingMessageHandler> handler,
      components_sharing_message::SharingMessage::PayloadCase payload_case);

  // Unregister SharingMessageHandler for |payload_case|.
  void UnregisterSharingHandler(
      components_sharing_message::SharingMessage::PayloadCase payload_case);

  // Sets a notification action handler for |notification_id|. Replaces any
  // previously set handlers for |notification_id|. |callback| may be a null
  // callback which clears the handler for |notification_id|.
  void SetNotificationActionHandler(const std::string& notification_id,
                                    NotificationActionCallback callback);

  // Returns the notification action handler for |notification_id| set by
  // SetNotificationActionHandler(). The returned callback may be null if no
  // handler has been set before for |notification_id|.
  NotificationActionCallback GetNotificationActionHandler(
      const std::string& notification_id) const;

  // Used to register devices with required capabilities in tests.
  void RegisterDeviceInTesting(
      std::set<sync_pb::SharingSpecificFields_EnabledFeatures> enabled_features,
      SharingDeviceRegistration::RegistrationCallback callback);

  SharingDeviceSource* GetDeviceSource() const;

  // Returns the current state of SharingService for testing.
  State GetStateForTesting() const;

  // Returns SharingSyncPreference for integration tests.
  SharingSyncPreference* GetSyncPreferencesForTesting() const;

  // Returns SharingFCMHandler for testing.
  SharingFCMHandler* GetFCMHandlerForTesting() const;

  // Returns SharingMessageSender for testing.
  SharingMessageSender* GetMessageSenderForTesting() const;

  // Returns the message handler registered for |payload_case| for testing.
  SharingMessageHandler* GetSharingHandlerForTesting(
      components_sharing_message::SharingMessage::PayloadCase payload_case)
      const;

  // SendTabToSelfModelObserver implementation.
  void EntriesAddedRemotely(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          new_entries) override {}
  void EntriesRemovedRemotely(const std::vector<std::string>& guids) override {}
  void SendTabToSelfModelLoaded() override {}
  void EntryAddedLocally(
      const send_tab_to_self::SendTabToSelfEntry* entry) override;

 private:
  // Overrides for syncer::SyncServiceObserver.
  void OnSyncShutdown(syncer::SyncService* sync) override;
  void OnStateChanged(syncer::SyncService* sync) override;

  void RefreshVapidKey();
  void RegisterDevice();
  void UnregisterDevice();

  void OnDeviceRegistered(SharingDeviceRegistrationResult result);
  void OnDeviceUnregistered(SharingDeviceRegistrationResult result);

  // Sends a push notification to users after they send a tab to one of their
  // iOS devices.
  void SendNotificationForSendTabToSelfPush(
      const send_tab_to_self::SendTabToSelfEntry& entry,
      const favicon_base::FaviconRawBitmapResult& result);

  std::unique_ptr<SharingSyncPreference> sync_prefs_;
  std::unique_ptr<VapidKeyManager> vapid_key_manager_;
  std::unique_ptr<SharingDeviceRegistration> sharing_device_registration_;
  std::unique_ptr<SharingMessageSender> message_sender_;
  const std::unique_ptr<SharingDeviceSource> device_source_;
  std::unique_ptr<SharingHandlerRegistry> handler_registry_;
  std::unique_ptr<SharingFCMHandler> fcm_handler_;

  raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<favicon::FaviconService> favicon_service_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  net::BackoffEntry backoff_entry_;
  State state_;

  base::CancelableTaskTracker task_tracker_;

  // Map of notification id to notification handler callback.
  std::map<std::string, NotificationActionCallback>
      notification_action_handlers_;

  base::ScopedObservation<send_tab_to_self::SendTabToSelfModel,
                          send_tab_to_self::SendTabToSelfModelObserver>
      send_tab_to_self_scoped_observation_{this};

  base::WeakPtrFactory<SharingService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_SERVICE_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_CLIENT_IMPL_H_
#define COMPONENTS_GCM_DRIVER_GCM_CLIENT_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/fcm/engine/checkin_info.h"
#include "components/fcm/engine/chrome_build_info.h"
#include "components/fcm/engine/fcm_internals_builder.h"
#include "components/fcm/engine/fcm_stats_recorder_impl.h"
#include "components/gcm_driver/gcm_client.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/engine/gservices_settings.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/engine/unregistration_request.h"
#include "google_apis/gcm/protocol/android_checkin.pb.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"

class GURL;

namespace base {
class Clock;
class Time;
}  // namespace base

namespace mcs_proto {
class DataMessageStanza;
}  // namespace mcs_proto

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace gcm {

class CheckinRequest;
class ConnectionFactory;
class GCMClientImplTest;
class GCMRegistrationManagerImpl;

// Implements the GCM Client. It is used to coordinate MCS Client (communication
// with MCS) and other pieces of GCM infrastructure like Registration and
// Checkins. It also allows for registering user delegates that host
// applications that send and receive messages.
class GCMClientImpl : public GCMClient,
                      public GCMStatsRecorder::Delegate,
                      public ConnectionFactory::ConnectionListener,
                      public MCSClient::Delegate {
 public:
  // State representation of the GCMClient.
  // Any change made to this enum should have corresponding change in the
  // GetStateString(...) function.
  enum State {
    // Uninitialized.
    UNINITIALIZED,
    // Initialized,
    INITIALIZED,
    // GCM store loading is in progress.
    LOADING,
    // GCM store is loaded.
    LOADED,
    // Initial device checkin is in progress.
    INITIAL_DEVICE_CHECKIN,
    // Ready to accept requests.
    READY,
  };

  explicit GCMClientImpl(
      std::unique_ptr<fcm::FcmInternalsBuilder> internals_builder);

  GCMClientImpl(const GCMClientImpl&) = delete;
  GCMClientImpl& operator=(const GCMClientImpl&) = delete;

  ~GCMClientImpl() override;

  // GCMClient implementation.
  void Initialize(
      const fcm::ChromeBuildInfo& chrome_build_info,
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      base::RepeatingCallback<void(
          mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
          get_socket_factory_callback,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      std::unique_ptr<Encryptor> encryptor) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void Start(StartMode start_mode) override;
  void Stop() override;
  void Register(
      scoped_refptr<GCMRegistrationInfo> gcm_registration_info) override;
  bool ValidateRegistration(scoped_refptr<RegistrationInfo> registration_info,
                            const std::string& registration_id) override;
  void Unregister(
      scoped_refptr<GCMRegistrationInfo> gcm_registration_info) override;
  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const OutgoingMessage& message) override;
  void RecordDecryptionFailure(const std::string& app_id,
                               fcm::FcmDecryptionResult result) override;
  void SetRecording(bool recording) override;
  void ClearActivityLogs() override;
  GCMStatistics GetStatistics() const override;
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id,
                         const std::string& extra_data) override;
  void RemoveInstanceIDData(const std::string& app_id) override;
  void GetInstanceIDData(const std::string& app_id,
                         std::string* instance_id,
                         std::string* extra_data) override;
  void GetToken(scoped_refptr<InstanceIDTokenInfo> token_info,
                GetTokenCallback callback) override;
  void DeleteToken(scoped_refptr<InstanceIDTokenInfo> token_info,
                   DeleteTokenCallback callback) override;
  void WaitForInitialConnectionReady(base::OnceClosure callback) override;

  // GCMStatsRecorder::Delegate implemenation.
  void OnActivityRecorded() override;

  // ConnectionFactory::ConnectionListener implementation.
  void OnConnected(const GURL& current_server,
                   const net::IPEndPoint& ip_endpoint) override;
  void OnDisconnected() override;

  // MCSClient::Delegate implementation.
  void OnMessageReceivedFromMCS(const MCSMessage& message) override;
  void OnMessageSentToMCS(const std::string& app_id,
                          const std::string& message_id,
                          MCSClient::MessageSendStatus status) override;

 private:
  // Collection of pending unregistration requests. Keys are RegistrationInfo
  // instance, while values are pending unregistration requests to disable the
  // registration ID currently assigned to the application.
  using PendingUnregistrationRequests =
      std::map<scoped_refptr<RegistrationInfo>,
               std::unique_ptr<UnregistrationRequest>,
               RegistrationInfoComparer>;

  friend class GCMClientImplTest;
  friend class GCMClientInstanceIDTest;

  // Returns text representation of the enum State.
  std::string GetStateString() const;

  // Runs after GCM Store load is done to trigger continuation of the
  // initialization.
  void OnLoadCompleted(std::unique_ptr<GCMStore::LoadResult> result);
  // Starts the GCM.
  void StartGCM();
  // Initializes mcs_client_, which handles the connection to MCS.
  void InitializeMCSClient();
  // Complets the first time device checkin.
  void OnFirstTimeDeviceCheckinCompleted(const fcm::CheckinInfo& checkin_info);
  // Starts a login on mcs_client_.
  void StartMCSLogin();
  // Resets the GCM store when it is corrupted.
  void ResetStore();
  // Sets state to ready. This will initiate the MCS login and notify the
  // delegates.
  void OnReady();

  // Starts a first time device checkin.
  void StartCheckin();
  // Completes the device checkin request by parsing the |checkin_response|.
  // Function also cleans up the pending checkin.
  void OnCheckinCompleted(
      net::HttpStatusCode response_code,
      const checkin_proto::AndroidCheckinResponse& checkin_response);

  // Callback passed to GCMStore::SetGServicesSettings.
  void SetGServicesSettingsCallback(bool success);

  // Schedules next periodic device checkin and makes sure there is at most one
  // pending checkin at a time. This function is meant to be called after a
  // successful checkin.
  void SchedulePeriodicCheckin();
  // Gets the time until next checkin.
  base::TimeDelta GetTimeToNextCheckin() const;
  // Callback for setting last checkin information in the |gcm_store_|.
  void SetLastCheckinInfoCallback(bool success);

  // Callback for persisting device credentials in the |gcm_store_|.
  void SetDeviceCredentialsCallback(bool success);

  // Callback for persisting registration info in the |gcm_store_|.
  void UpdateRegistrationCallback(bool success);

  // Callback for destroying the GCM store.
  void DestroyStoreCallback(bool success);

  // Callback for resetting the GCM store. The store will be reloaded.
  void ResetStoreCallback(bool success);

  // Notifies observers that a legacy GCM registration request has finished.
  void OnRegisterFinished(
      scoped_refptr<GCMRegistrationInfo> gcm_registration_info,
      const std::string& registration_id,
      Result result);

  // Removes a single matching registration or token from the cache and
  // persistent store, returning true if found.
  bool RemoveCachedRegistration(
      const scoped_refptr<RegistrationInfo>& registration_info);

  // Removes all cached Instance ID tokens for `app_id` from the cache and
  // persistent store, returning true if at least one token was removed.
  bool RemoveAllCachedInstanceIDTokens(const std::string& app_id);

  // Starts an unregistration request for `registration_info` and tracks it as
  // pending, invoking `callback` upon completion.
  void StartUnregistrationRequest(
      scoped_refptr<RegistrationInfo> registration_info,
      const std::string& category,
      const std::string& subtype,
      std::unique_ptr<UnregistrationRequest::CustomRequestHandler>
          request_handler,
      const std::string& source_to_record,
      base::OnceCallback<void(Result result)> callback);

  // Completes a pending `UnregistrationRequest`, invoking `callback` with the
  // mapped result.
  void OnUnregistrationRequestCompleted(
      scoped_refptr<RegistrationInfo> registration_info,
      base::OnceCallback<void(Result result)> callback,
      UnregistrationRequest::Status status);

  // Notifies observers that an `Unregister()` call has finished.
  void NotifyUnregisterFinished(
      scoped_refptr<GCMRegistrationInfo> gcm_registration_info,
      Result result);

  // Handles incoming data message and dispatches it to observers of this class.
  void HandleIncomingMessage(const gcm::MCSMessage& message);

  // Fires OnMessageReceived event on observers of this class, based on the
  // details in |data_message_stanza| and |message_data|.
  void HandleIncomingDataMessage(
      const std::string& app_id,
      const mcs_proto::DataMessageStanza& data_message_stanza,
      MessageData& message_data);

  // Fires OnMessagesDeleted event on observers of this class, based on the
  // details in `data_message_stanza`.
  void HandleIncomingDeletedMessages(
      const std::string& app_id,
      const mcs_proto::DataMessageStanza& data_message_stanza);

  // Fires OnMessageSendError event on observers of this class, based on the
  // details in |data_message_stanza| and |message_data|.
  void HandleIncomingSendError(
      const std::string& app_id,
      const mcs_proto::DataMessageStanza& data_message_stanza,
      MessageData& message_data);

  // Is there any standalone app being registered for GCM?
  bool HasStandaloneRegisteredApp() const;

  // Destroys the store when it is not needed.
  void DestroyStoreWhenNotNeeded();

  // Reset all cahced values.
  void ResetCache();

  // Builder for the GCM internals (mcs client, etc.).
  std::unique_ptr<fcm::FcmInternalsBuilder> internals_builder_;

  // Recorder that logs GCM activities.
  fcm::FcmStatsRecorderImpl recorder_;

  // State of the GCM Client Implementation.
  State state_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  // Flag to indicate if the GCM should be delay started until it is actually
  // used in either of the following cases:
  // 1) The GCM store contains the registration records.
  // 2) GCM functionailities are explicitly called.
  StartMode start_mode_;

  // Device checkin info (android ID and security token used by device).
  fcm::CheckinInfo device_checkin_info_;

  // Clock used for timing of retry logic. Passed in for testing.
  raw_ptr<base::Clock> clock_;

  // Information about the chrome build.
  // TODO(fgorski): Check if it can be passed in constructor and made const.
  fcm::ChromeBuildInfo chrome_build_info_;

  // Persistent data store for keeping device credentials, messages and user to
  // serial number mappings.
  std::unique_ptr<GCMStore> gcm_store_;

  // Manages registrations and Instance ID data. Must be declared after
  // `gcm_store_`, `clock_`, and `recorder_` so they outlive it.
  // TODO(b/545115204): Move ownership to `GCMDriverDesktop::IOWorker`.
  std::unique_ptr<GCMRegistrationManagerImpl> registration_manager_;

  // Data loaded from the GCM store.
  std::unique_ptr<GCMStore::LoadResult> load_result_;

  // Tracks if the GCM store has been reset. This is used to prevent from
  // resetting and loading from the store again and again.
  bool gcm_store_reset_;

  std::unique_ptr<ConnectionFactory> connection_factory_;
  base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
      get_socket_factory_callback_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // Controls receiving and sending of packets and reliable message queueing.
  // Must be destroyed before |network_session_|.
  std::unique_ptr<MCSClient> mcs_client_;

  std::unique_ptr<CheckinRequest> checkin_request_;

  // Currently pending unregistration requests. GCMClientImpl owns the
  // UnregistrationRequests.
  PendingUnregistrationRequests pending_unregistration_requests_;

  // G-services settings that were provided by MCS.
  GServicesSettings gservices_settings_;

  // Time of the last successful checkin.
  base::Time last_checkin_time_;

  // Factory for creating references when scheduling periodic checkin.
  base::WeakPtrFactory<GCMClientImpl> periodic_checkin_ptr_factory_{this};

  // Factory for wiping out GCM store.
  base::WeakPtrFactory<GCMClientImpl> destroying_gcm_store_ptr_factory_{this};

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<GCMClientImpl> weak_ptr_factory_{this};
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_CLIENT_IMPL_H_

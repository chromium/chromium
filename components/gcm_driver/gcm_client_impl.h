// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_stats_recorder_impl.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/engine/gservices_settings.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/engine/registration_request.h"
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

// Helper class for building GCM internals. Allows tests to inject fake versions
// as necessary.
class GCMInternalsBuilder {
 public:
  GCMInternalsBuilder();
  virtual ~GCMInternalsBuilder();

  virtual base::Clock* GetClock();
  virtual std::unique_ptr<MCSClient> BuildMCSClient(
      const std::string& version,
      base::Clock* clock,
      ConnectionFactory* connection_factory,
      GCMStore* gcm_store,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      GCMStatsRecorder* recorder);
  virtual std::unique_ptr<ConnectionFactory> BuildConnectionFactory(
      const std::vector<GURL>& endpoints,
      const net::BackoffEntry::Policy& backoff_policy,
      base::RepeatingCallback<void(
          mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
          get_socket_factory_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      GCMStatsRecorder* recorder,
      network::NetworkConnectionTracker* network_connection_tracker);
};

// Implements the GCM Client. It is used to coordinate MCS Client (communication
// with MCS) and other pieces of GCM infrastructure like Registration and
// Checkins. It also allows for registering user delegates that host
// applications that send and receive messages.
class GCMClientImpl
    : public GCMClient, public GCMStatsRecorder::Delegate,
      public ConnectionFactory::ConnectionListener {
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
      std::unique_ptr<GCMInternalsBuilder> internals_builder);
  ~GCMClientImpl() override;

  // GCMClient implementation.
  void Initialize(
      const ChromeBuildInfo& chrome_build_info,
      const base::FilePath& store_path,
      bool remove_account_mappings_with_email_key,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      base::RepeatingCallback<void(
          mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
          get_socket_factory_callback,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      std::unique_ptr<Encryptor> encryptor,
      GCMClient::Delegate* delegate) override;
  void Start(StartMode start_mode) override;
  void Stop() override;
  void Register(scoped_refptr<RegistrationInfo> registration_info) override;
  bool ValidateRegistration(scoped_refptr<RegistrationInfo> registration_info,
                            const std::string& registration_id) override;
  void Unregister(scoped_refptr<RegistrationInfo> registration_info) override;
  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const OutgoingMessage& message) override;
  void RecordDecryptionFailure(const std::string& app_id,
                               GCMDecryptionResult result) override;
  void SetRecording(bool recording) override;
  void ClearActivityLogs() override;
  GCMStatistics GetStatistics() const override;
  void SetAccountTokens(
      const std::vector<AccountTokenInfo>& account_tokens) override;
  void UpdateAccountMapping(const AccountMapping& account_mapping) override;
  void RemoveAccountMapping(const CoreAccountId& account_id) override;
  void SetLastTokenFetchTime(const base::Time& time) override;
  void UpdateHeartbeatTimer(
      std::unique_ptr<base::RetainingOneShotTimer> timer) override;
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id,
                         const std::string& extra_data) override;
  void RemoveInstanceIDData(const std::string& app_id) override;
  void GetInstanceIDData(const std::string& app_id,
                         std::string* instance_id,
                         std::string* extra_data) override;
  void AddHeartbeatInterval(const std::string& scope, int interval_ms) override;
  void RemoveHeartbeatInterval(const std::string& scope) override;

  // GCMStatsRecorder::Delegate implemenation.
  void OnActivityRecorded() override;

  // ConnectionFactory::ConnectionListener implementation.
  void OnConnected(const GURL& current_server,
                   const net::IPEndPoint& ip_endpoint) override;
  void OnDisconnected() override;

 private:
  // The check-in info for the device.
  // TODO(fgorski): Convert to a class with explicit getters/setters.
  struct CheckinInfo {
    CheckinInfo();
    ~CheckinInfo();
    bool IsValid() const { return android_id != 0 && secret != 0; }
    void SnapshotCheckinAccounts();
    void Reset();

    // Android ID of the device as assigned by the server.
    uint64_t android_id;
    // Security token of the device as assigned by the server.
    uint64_t secret;
    // True if accounts were already provided through SetAccountsForCheckin(),
    // or when |last_checkin_accounts| was loaded as empty.
    bool accounts_set;
    // Map of account email addresses and OAuth2 tokens that will be sent to the
    // checkin server on a next checkin.
    std::map<std::string, std::string> account_tokens;
    // As set of accounts last checkin was completed with.
    std::set<std::string> last_checkin_accounts;
  };

  // Reasons for resetting the GCM Store.
  // Note: this enum is recorded into a histogram. Do not change enum value
  // or order.
  enum ResetReason {
    LOAD_FAILURE,      // GCM store failed to load, but the store exists.
    CHECKIN_REJECTED,  // Checkin was rejected by server.

    RESET_REASON_COUNT,
  };

  // Collection of pending registration requests. Keys are RegistrationInfo
  // instance, while values are pending registration requests to obtain a
  // registration ID for requesting application.
  using PendingRegistrationRequests =
      std::map<scoped_refptr<RegistrationInfo>,
               std::unique_ptr<RegistrationRequest>,
               RegistrationInfoComparer>;

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

  // Callbacks for the MCSClient.
  // Receives messages and dispatches them to relevant user delegates.
  void OnMessageReceivedFromMCS(const gcm::MCSMessage& message);
  // Receives confirmation of sent messages or information about errors.
  void OnMessageSentToMCS(int64_t user_serial_number,
                          const std::string& app_id,
                          const std::string& message_id,
                          MCSClient::MessageSendStatus status);
  // Receives information about mcs_client_ errors.
  void OnMCSError();

  // Runs after GCM Store load is done to trigger continuation of the
  // initialization.
  void OnLoadCompleted(std::unique_ptr<GCMStore::LoadResult> result);
  // Starts the GCM.
  void StartGCM();
  // Initializes mcs_client_, which handles the connection to MCS.
  void InitializeMCSClient();
  // Complets the first time device checkin.
  void OnFirstTimeDeviceCheckinCompleted(const CheckinInfo& checkin_info);
  // Starts a login on mcs_client_.
  void StartMCSLogin();
  // Resets the GCM store when it is corrupted.
  void ResetStore();
  // Sets state to ready. This will initiate the MCS login and notify the
  // delegates.
  void OnReady(const std::vector<AccountMapping>& account_mappings,
               const base::Time& last_token_fetch_time);

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

  // Callback for all store operations that do not try to recover, if write in
  // |gcm_store_| fails.
  void DefaultStoreCallback(bool success);

  // Callback for store operation where result does not matter.
  void IgnoreWriteResultCallback(const std::string& operation_suffix_for_uma,
                                 bool success);

  // Callback for destroying the GCM store.
  void DestroyStoreCallback(bool success);

  // Callback for resetting the GCM store. The store will be reloaded.
  void ResetStoreCallback(bool success);

  // Completes the registration request.
  void OnRegisterCompleted(scoped_refptr<RegistrationInfo> registration_info,
                           RegistrationRequest::Status status,
                           const std::string& registration_id);

  // Completes the unregistration request.
  void OnUnregisterCompleted(scoped_refptr<RegistrationInfo> registration_info,
                             UnregistrationRequest::Status status);

  // Completes the GCM store destroy request.
  void OnGCMStoreDestroyed(bool success);

  // Handles incoming data message and dispatches it the delegate of this class.
  void HandleIncomingMessage(const gcm::MCSMessage& message);

  // Fires OnMessageReceived event on the delegate of this class, based on the
  // details in |data_message_stanza| and |message_data|.
  void HandleIncomingDataMessage(
      const std::string& app_id,
      bool was_subtype,
      const mcs_proto::DataMessageStanza& data_message_stanza,
      MessageData& message_data);

  // Fires OnMessagesDeleted event on the delegate of this class, based on the
  // details in |data_message_stanza| and |message_data|.
  void HandleIncomingDeletedMessages(
      const std::string& app_id,
      const mcs_proto::DataMessageStanza& data_message_stanza,
      MessageData& message_data);

  // Fires OnMessageSendError event on the delegate of this class, based on the
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
  std::unique_ptr<GCMInternalsBuilder> internals_builder_;

  // Recorder that logs GCM activities.
  GCMStatsRecorderImpl recorder_;

  // State of the GCM Client Implementation.
  State state_;

  GCMClient::Delegate* delegate_;

  // Flag to indicate if the GCM should be delay started until it is actually
  // used in either of the following cases:
  // 1) The GCM store contains the registration records.
  // 2) GCM functionailities are explicitly called.
  StartMode start_mode_;

  // Device checkin info (android ID and security token used by device).
  CheckinInfo device_checkin_info_;

  // Clock used for timing of retry logic. Passed in for testing.
  base::Clock* clock_;

  // Information about the chrome build.
  // TODO(fgorski): Check if it can be passed in constructor and made const.
  ChromeBuildInfo chrome_build_info_;

  // Persistent data store for keeping device credentials, messages and user to
  // serial number mappings.
  std::unique_ptr<GCMStore> gcm_store_;

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

  network::NetworkConnectionTracker* network_connection_tracker_;

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // Controls receiving and sending of packets and reliable message queueing.
  // Must be destroyed before |network_session_|.
  std::unique_ptr<MCSClient> mcs_client_;

  std::unique_ptr<CheckinRequest> checkin_request_;

  // Cached registration info.
  RegistrationInfoMap registrations_;

  // Currently pending registration requests. GCMClientImpl owns the
  // RegistrationRequests.
  PendingRegistrationRequests pending_registration_requests_;

  // Currently pending unregistration requests. GCMClientImpl owns the
  // UnregistrationRequests.
  PendingUnregistrationRequests pending_unregistration_requests_;

  // G-services settings that were provided by MCS.
  GServicesSettings gservices_settings_;

  // Time of the last successful checkin.
  base::Time last_checkin_time_;

  // Cached instance ID data, key is app ID and value is pair of instance ID
  // and extra data.
  std::map<std::string, std::pair<std::string, std::string>> instance_id_data_;

  // Factory for creating references when scheduling periodic checkin.
  base::WeakPtrFactory<GCMClientImpl> periodic_checkin_ptr_factory_{this};

  // Factory for wiping out GCM store.
  base::WeakPtrFactory<GCMClientImpl> destroying_gcm_store_ptr_factory_{this};

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<GCMClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GCMClientImpl);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_CLIENT_IMPL_H_

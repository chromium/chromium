// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_CLIENT_H_
#define COMPONENTS_GCM_DRIVER_GCM_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/gcm_activity.h"
#include "components/gcm_driver/registration_info.h"
#include "google_apis/gaia/core_account_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom-forward.h"

namespace base {
class FilePath;
class RetainingOneShotTimer;
class SequencedTaskRunner;
}  // namespace base

namespace net {
class IPEndPoint;
}  // namespace net

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace gcm {

struct AccountMapping;
class Encryptor;
enum class GCMDecryptionResult;

// Interface that encapsulates the network communications with the Google Cloud
// Messaging server. This interface is not supposed to be thread-safe.
class GCMClient {
 public:
  // Controls how GCM is being started. At first, GCMClient will be initialized
  // and GCM store will be loaded. Then GCM connection may or may not be
  // initiated depending on this enum value.
  enum StartMode {
    // GCM should be started only when it is being actually used. If no
    // registration record is found, GCM will not kick off.
    DELAYED_START,
    // GCM should be started immediately.
    IMMEDIATE_START
  };

  // Used for UMA. Can add enum values, but never renumber or delete and reuse.
  enum Result {
    // Successful operation.
    SUCCESS,
    // Invalid parameter.
    INVALID_PARAMETER,
    // GCM is disabled.
    GCM_DISABLED,
    // Previous asynchronous operation is still pending to finish. Certain
    // operation, like register, is only allowed one at a time.
    ASYNC_OPERATION_PENDING,
    // Network socket error.
    NETWORK_ERROR,
    // Problem at the server.
    SERVER_ERROR,
    // Exceeded the specified TTL during message sending.
    TTL_EXCEEDED,
    // Other errors.
    UNKNOWN_ERROR,

    // Used for UMA. Keep LAST_RESULT up to date and sync with histograms.xml.
    LAST_RESULT = UNKNOWN_ERROR
  };

  enum ChromePlatform {
    PLATFORM_WIN,
    PLATFORM_MAC,
    PLATFORM_LINUX,
    PLATFORM_CROS,
    PLATFORM_IOS,
    PLATFORM_ANDROID,
    PLATFORM_UNSPECIFIED
  };

  enum ChromeChannel {
    CHANNEL_STABLE,
    CHANNEL_BETA,
    CHANNEL_DEV,
    CHANNEL_CANARY,
    CHANNEL_UNKNOWN
  };

  struct ChromeBuildInfo {
    ChromeBuildInfo();
    ~ChromeBuildInfo();

    ChromePlatform platform;
    ChromeChannel channel;
    std::string version;
    std::string product_category_for_subtypes;
  };

  // Detailed information of the Send Error event.
  struct SendErrorDetails {
    SendErrorDetails();
    SendErrorDetails(const SendErrorDetails& other);
    ~SendErrorDetails();

    std::string message_id;
    MessageData additional_data;
    Result result;
  };

  // Internal states and activity statistics of a GCM client.
  struct GCMStatistics {
   public:
    GCMStatistics();
    GCMStatistics(const GCMStatistics& other);
    ~GCMStatistics();

    bool is_recording;
    bool gcm_client_created;
    std::string gcm_client_state;
    bool connection_client_created;
    std::string connection_state;
    base::Time last_checkin;
    base::Time next_checkin;
    uint64_t android_id;
    uint64_t android_secret;
    std::vector<std::string> registered_app_ids;
    int send_queue_size;
    int resend_queue_size;

    RecordedActivities recorded_activities;
  };

  // Information about account.
  struct AccountTokenInfo {
    CoreAccountId account_id;
    std::string email;
    std::string access_token;
  };

  // A delegate interface that allows the GCMClient instance to interact with
  // its caller, i.e. notifying asynchronous event.
  class Delegate {
   public:
    // Called when the registration completed successfully or an error occurs.
    // |registration_info|: the specific information required for the
    //                      registration.
    // |registration_id|: non-empty if the registration completed successfully.
    // |result|: the type of the error if an error occured, success otherwise.
    virtual void OnRegisterFinished(
        scoped_refptr<RegistrationInfo> registration_info,
        const std::string& registration_id,
        Result result) = 0;

    // Called when the unregistration completed.
    // |registration_info|: the specific information required for the
    //                      registration.
    // |result|: result of the unregistration.
    virtual void OnUnregisterFinished(
        scoped_refptr<RegistrationInfo> registration_info,
        GCMClient::Result result) = 0;

    // Called when the message is scheduled to send successfully or an error
    // occurs.
    // |app_id|: application ID.
    // |message_id|: ID of the message being sent.
    // |result|: the type of the error if an error occured, success otherwise.
    virtual void OnSendFinished(const std::string& app_id,
                                const std::string& message_id,
                                Result result) = 0;

    // Called when a message has been received.
    // |app_id|: application ID.
    // |message|: message received.
    virtual void OnMessageReceived(const std::string& app_id,
                                   const IncomingMessage& message) = 0;

    // Called when some messages have been deleted from the server.
    // |app_id|: application ID.
    virtual void OnMessagesDeleted(const std::string& app_id) = 0;

    // Called when a message failed to send to the server.
    // |app_id|: application ID.
    // |send_error_detials|: Details of the send error event, like mesasge ID.
    virtual void OnMessageSendError(
        const std::string& app_id,
        const SendErrorDetails& send_error_details) = 0;

    // Called when a message was acknowledged by the GCM server.
    // |app_id|: application ID.
    // |message_id|: ID of the acknowledged message.
    virtual void OnSendAcknowledged(const std::string& app_id,
                                    const std::string& message_id) = 0;

    // Called when the GCM becomes ready. To get to this state, GCMClient
    // finished loading from the GCM store and retrieved the device check-in
    // from the server if it hadn't yet.
    // |account_mappings|: a persisted list of accounts mapped to this GCM
    //                     client.
    // |last_token_fetch_time|: time of a last successful token fetch.
    virtual void OnGCMReady(const std::vector<AccountMapping>& account_mappings,
                            const base::Time& last_token_fetch_time) = 0;

    // Called when activities are being recorded and a new activity has just
    // been recorded.
    virtual void OnActivityRecorded() = 0;

    // Called when a new connection is established and a successful handshake
    // has been performed.
    virtual void OnConnected(const net::IPEndPoint& ip_endpoint) = 0;

    // Called when the connection is interrupted.
    virtual void OnDisconnected() = 0;

    // Called when the GCM store is reset (e.g. due to corruption), which
    // changes the device ID, invalidating all prior registrations.
    virtual void OnStoreReset() = 0;
  };

  GCMClient();
  virtual ~GCMClient();

  // Begins initialization of the GCM Client. This will not trigger a
  // connection. Must be called on |io_task_runner|.
  // |chrome_build_info|: chrome info, i.e., version, channel and etc.
  // |store_path|: path to the GCM store.
  // |blocking_task_runner|: for running blocking file tasks.
  // |io_task_runner|: for running IO tasks. When provided, it could be a
  //     wrapper on top of base::ThreadTaskRunnerHandle::Get() to provide power
  //     management featueres so that a delayed task posted to it can wake the
  //     system up from sleep to perform the task.
  // |get_socket_factory_callback|: a callback that can accept a receiver for a
  //     network::mojom::ProxyResolvingSocketFactory. It needs to be safe to
  //     run on any thread.
  // |delegate|: the delegate whose methods will be called asynchronously in
  //     response to events and messages.
  virtual void Initialize(
      const ChromeBuildInfo& chrome_build_info,
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      base::RepeatingCallback<void(
          mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
          get_socket_factory_callback,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker_,
      std::unique_ptr<Encryptor> encryptor,
      Delegate* delegate) = 0;

  // This will initiate the GCM connection only if |start_mode| means to start
  // the GCM immediately or the GCM registration records are found in the store.
  // Note that it is OK to call Start multiple times and the implementation
  // should handle it gracefully.
  virtual void Start(StartMode start_mode) = 0;

  // Stops using the GCM service. This will not erase the persisted data.
  virtual void Stop() = 0;

  // Registers with the server to access the provided service.
  // Delegate::OnRegisterFinished will be called asynchronously upon completion.
  // |registration_info|: the specific information required for the
  //                      registration. For GCM, it will contain app id and
  //                      sender IDs. For InstanceID, it will contain app_id,
  //                      authorized entity and scope.
  virtual void Register(scoped_refptr<RegistrationInfo> registration_info) = 0;

  // Checks that the provided |registration_id| (aka token for Instance ID
  // registrations) matches the stored registration info. Also checks sender IDs
  // match for GCM registrations.
  virtual bool ValidateRegistration(
      scoped_refptr<RegistrationInfo> registration_info,
      const std::string& registration_id) = 0;

  // Unregisters from the server to stop accessing the provided service.
  // Delegate::OnUnregisterFinished will be called asynchronously upon
  // completion.
  // |registration_info|: the specific information required for the
  //                      registration. For GCM, it will contain app id (sender
  //                      IDs can be ingored). For InstanceID, it will contain
  //                      app id, authorized entity and scope.
  virtual void Unregister(
      scoped_refptr<RegistrationInfo> registration_info) = 0;

  // Sends a message to a given receiver. Delegate::OnSendFinished will be
  // called asynchronously upon completion.
  // |app_id|: application ID.
  // |receiver_id|: registration ID of the receiver party.
  // |message|: message to be sent.
  virtual void Send(const std::string& app_id,
                    const std::string& receiver_id,
                    const OutgoingMessage& message) = 0;

  // Records a decryption failure due to |result| for the |app_id|.
  virtual void RecordDecryptionFailure(const std::string& app_id,
                                       GCMDecryptionResult result) = 0;

  // Enables or disables internal activity recording.
  virtual void SetRecording(bool recording) = 0;

  // Clear all recorded GCM activity logs.
  virtual void ClearActivityLogs() = 0;

  // Gets internal states and statistics.
  virtual GCMStatistics GetStatistics() const = 0;

  // Sets a list of accounts with OAuth2 tokens for the next checkin.
  // |account_tokens|: list of email addresses, account IDs and OAuth2 access
  //                   tokens.
  virtual void SetAccountTokens(
      const std::vector<AccountTokenInfo>& account_tokens) = 0;

  // Persists the |account_mapping| in the store.
  virtual void UpdateAccountMapping(const AccountMapping& account_mapping) = 0;

  // Removes the account mapping related to |account_id| from the persistent
  // store.
  virtual void RemoveAccountMapping(const CoreAccountId& account_id) = 0;

  // Sets last token fetch time in persistent store.
  virtual void SetLastTokenFetchTime(const base::Time& time) = 0;

  // Updates the timer used by the HeartbeatManager for sending heartbeats.
  virtual void UpdateHeartbeatTimer(
      std::unique_ptr<base::RetainingOneShotTimer> timer) = 0;

  // Adds the Instance ID data for a specific app to the persistent store.
  virtual void AddInstanceIDData(const std::string& app_id,
                                 const std::string& instance_id,
                                 const std::string& extra_data) = 0;

  // Removes the Instance ID data for a specific app from the persistent store.
  virtual void RemoveInstanceIDData(const std::string& app_id) = 0;

  // Retrieves the Instance ID data for a specific app from the persistent
  // store.
  virtual void GetInstanceIDData(const std::string& app_id,
                                 std::string* instance_id,
                                 std::string* extra_data) = 0;

  // Gets and sets custom heartbeat interval for the MCS connection.
  // |scope| is used to identify the component that requests a custom interval
  // to be set, and allows that component to later revoke the setting. It should
  // be unique.
  virtual void AddHeartbeatInterval(const std::string& scope,
                                    int interval_ms) = 0;
  virtual void RemoveHeartbeatInterval(const std::string& scope) = 0;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_CLIENT_H_

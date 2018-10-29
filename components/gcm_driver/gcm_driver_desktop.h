// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_DRIVER_DESKTOP_H_
#define COMPONENTS_GCM_DRIVER_GCM_DRIVER_DESKTOP_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/tuple.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_connection_observer.h"
#include "components/gcm_driver/gcm_driver.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"

class PrefService;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}

namespace gcm {

class GCMAccountMapper;
class GCMAppHandler;
class GCMClientFactory;
enum class GCMDecryptionResult;
class GCMDelayedTaskController;

// GCMDriver implementation for desktop and Chrome OS, using GCMClient.
class GCMDriverDesktop : public GCMDriver,
                         protected InstanceIDHandler {
 public:
  GCMDriverDesktop(
      std::unique_ptr<GCMClientFactory> gcm_client_factory,
      const GCMClient::ChromeBuildInfo& chrome_build_info,
      const std::string& channel_status_request_url,
      const std::string& user_agent,
      PrefService* prefs,
      const base::FilePath& store_path,
      base::RepeatingCallback<
          void(network::mojom::ProxyResolvingSocketFactoryRequest)>
          get_socket_factory_callback,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_for_ui,
      network::NetworkConnectionTracker* network_connection_tracker,
      const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
      const scoped_refptr<base::SequencedTaskRunner>& io_thread,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);
  ~GCMDriverDesktop() override;

  // GCMDriver implementation:
  void ValidateRegistration(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids,
      const std::string& registration_id,
      const ValidateRegistrationCallback& callback) override;
  void Shutdown() override;
  void OnSignedIn() override;
  void OnSignedOut() override;
  void AddAppHandler(const std::string& app_id,
                     GCMAppHandler* handler) override;
  void RemoveAppHandler(const std::string& app_id) override;
  void AddConnectionObserver(GCMConnectionObserver* observer) override;
  void RemoveConnectionObserver(GCMConnectionObserver* observer) override;
  void Enable() override;
  void Disable() override;
  GCMClient* GetGCMClientForTesting() const override;
  bool IsStarted() const override;
  bool IsConnected() const override;
  void GetGCMStatistics(const GetGCMStatisticsCallback& callback,
                        ClearActivityLogs clear_logs) override;
  void SetGCMRecording(const GetGCMStatisticsCallback& callback,
                       bool recording) override;
  void SetAccountTokens(
      const std::vector<GCMClient::AccountTokenInfo>& account_tokens) override;
  void UpdateAccountMapping(const AccountMapping& account_mapping) override;
  void RemoveAccountMapping(const std::string& account_id) override;
  base::Time GetLastTokenFetchTime() override;
  void SetLastTokenFetchTime(const base::Time& time) override;
  void WakeFromSuspendForHeartbeat(bool wake) override;
  InstanceIDHandler* GetInstanceIDHandlerInternal() override;
  void AddHeartbeatInterval(const std::string& scope, int interval_ms) override;
  void RemoveHeartbeatInterval(const std::string& scope) override;

  // Exposed for testing purpose.
  bool gcm_enabled() const { return gcm_enabled_; }
  GCMChannelStatusSyncer* gcm_channel_status_syncer_for_testing() {
    return gcm_channel_status_syncer_.get();
  }

 protected:
  // GCMDriver implementation:
  GCMClient::Result EnsureStarted(GCMClient::StartMode start_mode) override;
  void RegisterImpl(const std::string& app_id,
                    const std::vector<std::string>& sender_ids) override;
  void UnregisterImpl(const std::string& app_id) override;
  void SendImpl(const std::string& app_id,
                const std::string& receiver_id,
                const OutgoingMessage& message) override;
  void RecordDecryptionFailure(const std::string& app_id,
                               GCMDecryptionResult result) override;

  // InstanceIDHandler implementation:
  void GetToken(const std::string& app_id,
                const std::string& authorized_entity,
                const std::string& scope,
                const std::map<std::string, std::string>& options,
                const GetTokenCallback& callback) override;
  void ValidateToken(const std::string& app_id,
                     const std::string& authorized_entity,
                     const std::string& scope,
                     const std::string& token,
                     const ValidateTokenCallback& callback) override;
  void DeleteToken(const std::string& app_id,
                   const std::string& authorized_entity,
                   const std::string& scope,
                   const DeleteTokenCallback& callback) override;
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id,
                         const std::string& extra_data) override;
  void RemoveInstanceIDData(const std::string& app_id) override;
  void GetInstanceIDData(const std::string& app_id,
                         const GetInstanceIDDataCallback& callback) override;

 private:
  class IOWorker;

  typedef std::tuple<std::string, std::string, std::string> TokenTuple;
  struct TokenTupleComparer {
    bool operator()(const TokenTuple& a, const TokenTuple& b) const;
  };

  void DoValidateRegistration(
      std::unique_ptr<RegistrationInfo> registration_info,
      const std::string& registration_id,
      const ValidateRegistrationCallback& callback);

  //  Stops the GCM service. It can be restarted by calling EnsureStarted again.
  void Stop();

  // Remove cached data when GCM service is stopped.
  void RemoveCachedData();

  void DoRegister(const std::string& app_id,
                  const std::vector<std::string>& sender_ids);
  void DoUnregister(const std::string& app_id);
  void DoSend(const std::string& app_id,
              const std::string& receiver_id,
              const OutgoingMessage& message);
  void DoAddInstanceIDData(const std::string& app_id,
                           const std::string& instance_id,
                           const std::string& extra_data);
  void DoRemoveInstanceIDData(const std::string& app_id);
  void DoGetInstanceIDData(const std::string& app_id);
  void DoGetToken(const std::string& app_id,
                  const std::string& authorized_entity,
                  const std::string& scope,
                  const std::map<std::string, std::string>& options);
  void DoDeleteToken(const std::string& app_id,
                     const std::string& authorized_entity,
                     const std::string& scope);

  // Callbacks posted from IO thread to UI thread.
  void MessageReceived(const std::string& app_id,
                       const IncomingMessage& message);
  void MessagesDeleted(const std::string& app_id);
  void MessageSendError(const std::string& app_id,
                        const GCMClient::SendErrorDetails& send_error_details);
  void SendAcknowledged(const std::string& app_id,
                        const std::string& message_id);
  void GCMClientReady(const std::vector<AccountMapping>& account_mappings,
                      const base::Time& last_token_fetch_time);
  void OnConnected(const net::IPEndPoint& ip_endpoint);
  void OnDisconnected();
  void OnStoreReset();

  void GetGCMStatisticsFinished(const GCMClient::GCMStatistics& stats);
  void GetInstanceIDDataFinished(const std::string& app_id,
                                 const std::string& instance_id,
                                 const std::string& extra_data);
  void GetTokenFinished(const std::string& app_id,
                        const std::string& authorized_entity,
                        const std::string& scope,
                        const std::string& token,
                        GCMClient::Result result);
  void DeleteTokenFinished(const std::string& app_id,
                           const std::string& authorized_entity,
                           const std::string& scope,
                           GCMClient::Result result);

  std::unique_ptr<GCMChannelStatusSyncer> gcm_channel_status_syncer_;

  // Flag to indicate whether the user is signed in to a GAIA account.
  bool signed_in_;

  // Flag to indicate if GCM is started.
  bool gcm_started_;

  // Flag to indicate if GCM is enabled.
  bool gcm_enabled_;

  // Flag to indicate the last known state of the GCM client. Because this
  // flag lives on the UI thread, while the GCM client lives on the IO thread,
  // it may be out of date while connection changes are happening.
  bool connected_;

  // List of observers to notify when connection state changes.
  base::ObserverList<GCMConnectionObserver, false>::Unchecked
      connection_observer_list_;

  // Account mapper. Only works when user is signed in.
  std::unique_ptr<GCMAccountMapper> account_mapper_;

  // Time of last token fetching.
  base::Time last_token_fetch_time_;

  scoped_refptr<base::SequencedTaskRunner> ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> io_thread_;

  std::unique_ptr<GCMDelayedTaskController> delayed_task_controller_;

  // Whether the HeartbeatManager should try to wake the system from suspend for
  // sending heartbeat messages.
  bool wake_from_suspend_enabled_;

  // For all the work occurring on the IO thread. Must be destroyed on the IO
  // thread.
  std::unique_ptr<IOWorker> io_worker_;

  // Callback for GetGCMStatistics.
  GetGCMStatisticsCallback request_gcm_statistics_callback_;

  // Callbacks for GetInstanceIDData.
  std::map<std::string, GetInstanceIDDataCallback>
      get_instance_id_data_callbacks_;

  // Callbacks for GetToken/DeleteToken.
  std::map<TokenTuple, GetTokenCallback, TokenTupleComparer>
      get_token_callbacks_;
  std::map<TokenTuple, DeleteTokenCallback, TokenTupleComparer>
      delete_token_callbacks_;

  // Used to pass a weak pointer to the IO worker.
  base::WeakPtrFactory<GCMDriverDesktop> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMDriverDesktop);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DRIVER_DESKTOP_H_

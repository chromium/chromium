// Copyright 2014 The Chromium Authors
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
#include "base/containers/queue.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/tuple.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_connection_observer.h"
#include "components/gcm_driver/gcm_driver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
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
      PrefService* prefs,
      const base::FilePath& store_path,
      base::RepeatingCallback<void(
          mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
          get_socket_factory_callback,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_for_ui,
      network::NetworkConnectionTracker* network_connection_tracker,
      const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
      const scoped_refptr<base::SequencedTaskRunner>& io_thread,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

  GCMDriverDesktop(const GCMDriverDesktop&) = delete;
  GCMDriverDesktop& operator=(const GCMDriverDesktop&) = delete;

  ~GCMDriverDesktop() override;

  // GCMDriver implementation:
  void ValidateRegistration(const std::string& app_id,
                            const std::vector<std::string>& sender_ids,
                            const std::string& registration_id,
                            ValidateRegistrationCallback callback) override;
  void Shutdown() override;
  void AddAppHandler(const std::string& app_id,
                     GCMAppHandler* handler) override;
  void RemoveAppHandler(const std::string& app_id) override;
  void AddConnectionObserver(GCMConnectionObserver* observer) override;
  void RemoveConnectionObserver(GCMConnectionObserver* observer) override;
  GCMClient* GetGCMClientForTesting() const override;
  bool IsStarted() const override;
  bool IsConnected() const override;
  void GetGCMStatistics(GetGCMStatisticsCallback callback,
                        ClearActivityLogs clear_logs) override;
  void SetGCMRecording(const GCMStatisticsRecordingCallback& callback,
                       bool recording) override;
  void SetAccountTokens(
      const std::vector<GCMClient::AccountTokenInfo>& account_tokens) override;
  void UpdateAccountMapping(const AccountMapping& account_mapping) override;
  void RemoveAccountMapping(const CoreAccountId& account_id) override;
  base::Time GetLastTokenFetchTime() override;
  void SetLastTokenFetchTime(const base::Time& time) override;
  InstanceIDHandler* GetInstanceIDHandlerInternal() override;
  void AddHeartbeatInterval(const std::string& scope, int interval_ms) override;
  void RemoveHeartbeatInterval(const std::string& scope) override;

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
                base::TimeDelta time_to_live,
                GetTokenCallback callback) override;
  void ValidateToken(const std::string& app_id,
                     const std::string& authorized_entity,
                     const std::string& scope,
                     const std::string& token,
                     ValidateTokenCallback callback) override;
  void DeleteToken(const std::string& app_id,
                   const std::string& authorized_entity,
                   const std::string& scope,
                   DeleteTokenCallback callback) override;
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id,
                         const std::string& extra_data) override;
  void RemoveInstanceIDData(const std::string& app_id) override;
  void GetInstanceIDData(const std::string& app_id,
                         GetInstanceIDDataCallback callback) override;

 private:
  class IOWorker;

  typedef std::tuple<std::string, std::string, std::string> TokenTuple;
  struct TokenTupleComparer {
    bool operator()(const TokenTuple& a, const TokenTuple& b) const;
  };

  void DoValidateRegistration(scoped_refptr<RegistrationInfo> registration_info,
                              const std::string& registration_id,
                              ValidateRegistrationCallback callback);

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
                  base::TimeDelta time_to_live);
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
  void OnActivityRecorded(const GCMClient::GCMStatistics& stats);

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

  // Flag to indicate if GCM is started.
  bool gcm_started_;

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

  // For all the work occurring on the IO thread. Must be destroyed on the IO
  // thread.
  std::unique_ptr<IOWorker> io_worker_;

  // Callback for SetGCMRecording.
  GCMStatisticsRecordingCallback gcm_statistics_recording_callback_;

  // Callbacks for GetInstanceIDData. Initializing InstanceID is asynchronous,
  // which leads to a race condition when recreating an InstanceID before such
  // initialization has finished, causing multiple callbacks to be in flight.
  // Expecting all InstanceID users to care for that is fragile and complicated,
  // so allow for a queue of callbacks to be stored here instead.
  //
  // Note that other InstanceID callbacks don't have this concern, as they all
  // wait for initialization of the InstanceID instance to have completed.
  std::map<std::string, base::queue<GetInstanceIDDataCallback>>
      get_instance_id_data_callbacks_;

  // Callbacks for GetToken/DeleteToken.
  std::map<TokenTuple, GetTokenCallback, TokenTupleComparer>
      get_token_callbacks_;
  std::map<TokenTuple, DeleteTokenCallback, TokenTupleComparer>
      delete_token_callbacks_;

  // Used to pass a weak pointer to the IO worker.
  base::WeakPtrFactory<GCMDriverDesktop> weak_ptr_factory_{this};
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DRIVER_DESKTOP_H_

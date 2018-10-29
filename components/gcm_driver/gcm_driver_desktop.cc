// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_desktop.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_account_mapper.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_delayed_task_controller.h"
#include "components/gcm_driver/instance_id/instance_id_impl.h"
#include "components/gcm_driver/system_encryptor.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_CHROMEOS)
#include "components/timers/alarm_timer_chromeos.h"
#endif

namespace gcm {

class GCMDriverDesktop::IOWorker : public GCMClient::Delegate {
 public:
  // Called on UI thread.
  IOWorker(const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
           const scoped_refptr<base::SequencedTaskRunner>& io_thread);
  virtual ~IOWorker();

  // Overridden from GCMClient::Delegate:
  // Called on IO thread.
  void OnRegisterFinished(const linked_ptr<RegistrationInfo>& registration_info,
                          const std::string& registration_id,
                          GCMClient::Result result) override;
  void OnUnregisterFinished(
      const linked_ptr<RegistrationInfo>& registration_info,
      GCMClient::Result result) override;
  void OnSendFinished(const std::string& app_id,
                      const std::string& message_id,
                      GCMClient::Result result) override;
  void OnMessageReceived(const std::string& app_id,
                         const IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnMessageSendError(
      const std::string& app_id,
      const GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  void OnGCMReady(const std::vector<AccountMapping>& account_mappings,
                  const base::Time& last_token_fetch_time) override;
  void OnActivityRecorded() override;
  void OnConnected(const net::IPEndPoint& ip_endpoint) override;
  void OnDisconnected() override;
  void OnStoreReset() override;

  // Called on IO thread.
  void Initialize(
      std::unique_ptr<GCMClientFactory> gcm_client_factory,
      const GCMClient::ChromeBuildInfo& chrome_build_info,
      const base::FilePath& store_path,
      base::RepeatingCallback<
          void(network::mojom::ProxyResolvingSocketFactoryRequest)>
          get_socket_factory_callback,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> loader_factory_info,
      network::NetworkConnectionTracker* network_connection_tracker,
      const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
  void Start(GCMClient::StartMode start_mode,
             const base::WeakPtr<GCMDriverDesktop>& service);
  void Stop();
  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids);
  void Unregister(const std::string& app_id);
  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const OutgoingMessage& message);
  void GetGCMStatistics(GCMDriver::ClearActivityLogs clear_logs);
  void SetGCMRecording(bool recording);

  void SetAccountTokens(
      const std::vector<GCMClient::AccountTokenInfo>& account_tokens);
  void UpdateAccountMapping(const AccountMapping& account_mapping);
  void RemoveAccountMapping(const std::string& account_id);
  void SetLastTokenFetchTime(const base::Time& time);
  void WakeFromSuspendForHeartbeat(bool wake);
  void AddHeartbeatInterval(const std::string& scope, int interval_ms);
  void RemoveHeartbeatInterval(const std::string& scope);

  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id,
                         const std::string& extra_data);
  void RemoveInstanceIDData(const std::string& app_id);
  void GetInstanceIDData(const std::string& app_id);
  void GetToken(const std::string& app_id,
                const std::string& authorized_entity,
                const std::string& scope,
                const std::map<std::string, std::string>& options);
  bool ValidateRegistration(std::unique_ptr<RegistrationInfo> registration_info,
                            const std::string& registration_id);
  void DeleteToken(const std::string& app_id,
                   const std::string& authorized_entity,
                   const std::string& scope);

  void RecordDecryptionFailure(const std::string& app_id,
                               GCMDecryptionResult result);

  // For testing purpose. Can be called from UI thread. Use with care.
  GCMClient* gcm_client_for_testing() const { return gcm_client_.get(); }

 private:
  scoped_refptr<base::SequencedTaskRunner> ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> io_thread_;

  base::WeakPtr<GCMDriverDesktop> service_;

  std::unique_ptr<GCMClient> gcm_client_;

  DISALLOW_COPY_AND_ASSIGN(IOWorker);
};

GCMDriverDesktop::IOWorker::IOWorker(
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& io_thread)
    : ui_thread_(ui_thread),
      io_thread_(io_thread) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
}

GCMDriverDesktop::IOWorker::~IOWorker() {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
}

void GCMDriverDesktop::IOWorker::Initialize(
    std::unique_ptr<GCMClientFactory> gcm_client_factory,
    const GCMClient::ChromeBuildInfo& chrome_build_info,
    const base::FilePath& store_path,
    base::RepeatingCallback<
        void(network::mojom::ProxyResolvingSocketFactoryRequest)>
        get_socket_factory_callback,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> loader_factory_info,
    network::NetworkConnectionTracker* network_connection_tracker,
    const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  gcm_client_ = gcm_client_factory->BuildInstance();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_for_io =
      network::SharedURLLoaderFactory::Create(std::move(loader_factory_info));

  gcm_client_->Initialize(chrome_build_info, store_path, blocking_task_runner,
                          std::move(get_socket_factory_callback),
                          url_loader_factory_for_io, network_connection_tracker,
                          std::make_unique<SystemEncryptor>(), this);
}

void GCMDriverDesktop::IOWorker::OnRegisterFinished(
    const linked_ptr<RegistrationInfo>& registration_info,
    const std::string& registration_id,
    GCMClient::Result result) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  const GCMRegistrationInfo* gcm_registration_info =
      GCMRegistrationInfo::FromRegistrationInfo(registration_info.get());
  if (gcm_registration_info) {
    ui_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&GCMDriverDesktop::RegisterFinished, service_,
                       gcm_registration_info->app_id, registration_id, result));
  }

  const InstanceIDTokenInfo* instance_id_token_info =
      InstanceIDTokenInfo::FromRegistrationInfo(registration_info.get());
  if (instance_id_token_info) {
    ui_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&GCMDriverDesktop::GetTokenFinished, service_,
                       instance_id_token_info->app_id,
                       instance_id_token_info->authorized_entity,
                       instance_id_token_info->scope, registration_id, result));
  }
}

void GCMDriverDesktop::IOWorker::OnUnregisterFinished(
    const linked_ptr<RegistrationInfo>& registration_info,
    GCMClient::Result result) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  const GCMRegistrationInfo* gcm_registration_info =
      GCMRegistrationInfo::FromRegistrationInfo(registration_info.get());
  if (gcm_registration_info) {
    ui_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&GCMDriverDesktop::RemoveEncryptionInfoAfterUnregister,
                       service_, gcm_registration_info->app_id, result));
  }

  const InstanceIDTokenInfo* instance_id_token_info =
      InstanceIDTokenInfo::FromRegistrationInfo(registration_info.get());
  if (instance_id_token_info) {
    ui_thread_->PostTask(
        FROM_HERE, base::BindOnce(&GCMDriverDesktop::DeleteTokenFinished,
                                  service_, instance_id_token_info->app_id,
                                  instance_id_token_info->authorized_entity,
                                  instance_id_token_info->scope, result));
  }
}

void GCMDriverDesktop::IOWorker::OnSendFinished(const std::string& app_id,
                                                const std::string& message_id,
                                                GCMClient::Result result) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  ui_thread_->PostTask(FROM_HERE,
                       base::BindOnce(&GCMDriverDesktop::SendFinished, service_,
                                      app_id, message_id, result));
}

void GCMDriverDesktop::IOWorker::OnMessageReceived(
    const std::string& app_id,
    const IncomingMessage& message) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  ui_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::MessageReceived, service_,
                                app_id, message));
}

void GCMDriverDesktop::IOWorker::OnMessagesDeleted(const std::string& app_id) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  ui_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::MessagesDeleted, service_, app_id));
}

void GCMDriverDesktop::IOWorker::OnMessageSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  ui_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::MessageSendError, service_,
                                app_id, send_error_details));
}

void GCMDriverDesktop::IOWorker::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  ui_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::SendAcknowledged, service_,
                                app_id, message_id));
}

void GCMDriverDesktop::IOWorker::OnGCMReady(
    const std::vector<AccountMapping>& account_mappings,
    const base::Time& last_token_fetch_time) {
  ui_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::GCMClientReady, service_,
                                account_mappings, last_token_fetch_time));
}

void GCMDriverDesktop::IOWorker::OnActivityRecorded() {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
  // When an activity is recorded, get all the stats and refresh the UI of
  // gcm-internals page.
  GetGCMStatistics(GCMDriver::KEEP_LOGS);
}

void GCMDriverDesktop::IOWorker::OnConnected(
    const net::IPEndPoint& ip_endpoint) {
  ui_thread_->PostTask(FROM_HERE, base::BindOnce(&GCMDriverDesktop::OnConnected,
                                                 service_, ip_endpoint));
}

void GCMDriverDesktop::IOWorker::OnDisconnected() {
  ui_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::OnDisconnected, service_));
}

void GCMDriverDesktop::IOWorker::OnStoreReset() {
  ui_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::OnStoreReset, service_));
}

void GCMDriverDesktop::IOWorker::Start(
    GCMClient::StartMode start_mode,
    const base::WeakPtr<GCMDriverDesktop>& service) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  service_ = service;
  gcm_client_->Start(start_mode);
}

void GCMDriverDesktop::IOWorker::Stop() {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  gcm_client_->Stop();
}

void GCMDriverDesktop::IOWorker::Register(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  auto gcm_info = std::make_unique<GCMRegistrationInfo>();
  gcm_info->app_id = app_id;
  gcm_info->sender_ids = sender_ids;
  gcm_client_->Register(make_linked_ptr<RegistrationInfo>(gcm_info.release()));
}

bool GCMDriverDesktop::IOWorker::ValidateRegistration(
    std::unique_ptr<RegistrationInfo> registration_info,
    const std::string& registration_id) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  return gcm_client_->ValidateRegistration(
      make_linked_ptr(registration_info.release()), registration_id);
}

void GCMDriverDesktop::IOWorker::Unregister(const std::string& app_id) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  auto gcm_info = std::make_unique<GCMRegistrationInfo>();
  gcm_info->app_id = app_id;
  gcm_client_->Unregister(
      make_linked_ptr<RegistrationInfo>(gcm_info.release()));
}

void GCMDriverDesktop::IOWorker::Send(const std::string& app_id,
                                      const std::string& receiver_id,
                                      const OutgoingMessage& message) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  gcm_client_->Send(app_id, receiver_id, message);
}

void GCMDriverDesktop::IOWorker::GetGCMStatistics(
    ClearActivityLogs clear_logs) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
  gcm::GCMClient::GCMStatistics stats;

  if (gcm_client_) {
    if (clear_logs == GCMDriver::CLEAR_LOGS)
      gcm_client_->ClearActivityLogs();
    stats = gcm_client_->GetStatistics();
  }

  ui_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::GetGCMStatisticsFinished,
                                service_, stats));
}

void GCMDriverDesktop::IOWorker::SetGCMRecording(bool recording) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
  gcm::GCMClient::GCMStatistics stats;

  if (gcm_client_) {
    gcm_client_->SetRecording(recording);
    stats = gcm_client_->GetStatistics();
    stats.gcm_client_created = true;
  }

  ui_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::GetGCMStatisticsFinished,
                                service_, stats));
}

void GCMDriverDesktop::IOWorker::SetAccountTokens(
    const std::vector<GCMClient::AccountTokenInfo>& account_tokens) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  if (gcm_client_)
    gcm_client_->SetAccountTokens(account_tokens);
}

void GCMDriverDesktop::IOWorker::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  if (gcm_client_)
    gcm_client_->UpdateAccountMapping(account_mapping);
}

void GCMDriverDesktop::IOWorker::RemoveAccountMapping(
    const std::string& account_id) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  if (gcm_client_)
    gcm_client_->RemoveAccountMapping(account_id);
}

void GCMDriverDesktop::IOWorker::SetLastTokenFetchTime(const base::Time& time) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  if (gcm_client_)
    gcm_client_->SetLastTokenFetchTime(time);
}

void GCMDriverDesktop::IOWorker::AddInstanceIDData(
    const std::string& app_id,
    const std::string& instance_id,
    const std::string& extra_data) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  if (gcm_client_)
    gcm_client_->AddInstanceIDData(app_id, instance_id, extra_data);
}

void GCMDriverDesktop::IOWorker::RemoveInstanceIDData(
    const std::string& app_id) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  if (gcm_client_)
    gcm_client_->RemoveInstanceIDData(app_id);
}

void GCMDriverDesktop::IOWorker::GetInstanceIDData(
    const std::string& app_id) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  std::string instance_id;
  std::string extra_data;
  if (gcm_client_)
    gcm_client_->GetInstanceIDData(app_id, &instance_id, &extra_data);

  ui_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::GetInstanceIDDataFinished,
                                service_, app_id, instance_id, extra_data));
}

void GCMDriverDesktop::IOWorker::GetToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope,
    const std::map<std::string, std::string>& options) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  auto instance_id_token_info = std::make_unique<InstanceIDTokenInfo>();
  instance_id_token_info->app_id = app_id;
  instance_id_token_info->authorized_entity = authorized_entity;
  instance_id_token_info->scope = scope;
  instance_id_token_info->options = options;
  gcm_client_->Register(
      make_linked_ptr<RegistrationInfo>(instance_id_token_info.release()));
}

void GCMDriverDesktop::IOWorker::DeleteToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope) {
  auto instance_id_token_info = std::make_unique<InstanceIDTokenInfo>();
  instance_id_token_info->app_id = app_id;
  instance_id_token_info->authorized_entity = authorized_entity;
  instance_id_token_info->scope = scope;
  gcm_client_->Unregister(
      make_linked_ptr<RegistrationInfo>(instance_id_token_info.release()));
}

void GCMDriverDesktop::IOWorker::WakeFromSuspendForHeartbeat(bool wake) {
#if defined(OS_CHROMEOS)
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  std::unique_ptr<base::RetainingOneShotTimer> timer;
  if (wake)
    timer = std::make_unique<timers::SimpleAlarmTimer>();
  else
    timer = std::make_unique<base::RetainingOneShotTimer>();

  gcm_client_->UpdateHeartbeatTimer(std::move(timer));
#endif
}

void GCMDriverDesktop::IOWorker::AddHeartbeatInterval(const std::string& scope,
                                                      int interval_ms) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
  gcm_client_->AddHeartbeatInterval(scope, interval_ms);
}

void GCMDriverDesktop::IOWorker::RemoveHeartbeatInterval(
    const std::string& scope) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
  gcm_client_->RemoveHeartbeatInterval(scope);
}

void GCMDriverDesktop::IOWorker::RecordDecryptionFailure(
    const std::string& app_id,
    GCMDecryptionResult result) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
  gcm_client_->RecordDecryptionFailure(app_id, result);
}

GCMDriverDesktop::GCMDriverDesktop(
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
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : GCMDriver(store_path, blocking_task_runner),
      gcm_channel_status_syncer_(
          new GCMChannelStatusSyncer(this,
                                     prefs,
                                     channel_status_request_url,
                                     user_agent,
                                     url_loader_factory_for_ui)),
      signed_in_(false),
      gcm_started_(false),
      gcm_enabled_(true),
      connected_(false),
      account_mapper_(new GCMAccountMapper(this)),
      // Setting to max, to make sure it does not prompt for token reporting
      // Before reading a reasonable value from the DB, which might be never,
      // in which case the fetching will be triggered.
      last_token_fetch_time_(base::Time::Max()),
      ui_thread_(ui_thread),
      io_thread_(io_thread),
      wake_from_suspend_enabled_(false),
      weak_ptr_factory_(this) {
  gcm_enabled_ = gcm_channel_status_syncer_->gcm_enabled();

  // Create and initialize the GCMClient. Note that this does not initiate the
  // GCM check-in.
  io_worker_.reset(new IOWorker(ui_thread, io_thread));
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GCMDriverDesktop::IOWorker::Initialize,
          base::Unretained(io_worker_.get()), std::move(gcm_client_factory),
          chrome_build_info, store_path, std::move(get_socket_factory_callback),
          // ->Clone() permits creation of an equivalent
          // SharedURLLoaderFactory on IO thread.
          url_loader_factory_for_ui->Clone(),
          base::Unretained(network_connection_tracker), blocking_task_runner));
}

GCMDriverDesktop::~GCMDriverDesktop() {
}

void GCMDriverDesktop::ValidateRegistration(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const std::string& registration_id,
    const ValidateRegistrationCallback& callback) {
  DCHECK(!app_id.empty());
  DCHECK(!sender_ids.empty() && sender_ids.size() <= kMaxSenders);
  DCHECK(!registration_id.empty());
  DCHECK(!callback.is_null());
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    // Can't tell whether the registration is valid or not, so don't run the
    // callback (let it hang indefinitely).
    return;
  }

  // Only validating current state, so ignore pending register_callbacks_.

  auto gcm_info = std::make_unique<GCMRegistrationInfo>();
  gcm_info->app_id = app_id;
  gcm_info->sender_ids = sender_ids;
  // Normalize the sender IDs by making them sorted.
  std::sort(gcm_info->sender_ids.begin(), gcm_info->sender_ids.end());

  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::DoValidateRegistration,
                   weak_ptr_factory_.GetWeakPtr(), base::Passed(&gcm_info),
                   registration_id, callback));
    return;
  }

  DoValidateRegistration(std::move(gcm_info), registration_id, callback);
}

void GCMDriverDesktop::DoValidateRegistration(
    std::unique_ptr<RegistrationInfo> registration_info,
    const std::string& registration_id,
    const ValidateRegistrationCallback& callback) {
  base::PostTaskAndReplyWithResult(
      io_thread_.get(), FROM_HERE,
      base::Bind(&GCMDriverDesktop::IOWorker::ValidateRegistration,
                 base::Unretained(io_worker_.get()),
                 base::Passed(&registration_info), registration_id),
      callback);
}

void GCMDriverDesktop::Shutdown() {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  Stop();
  GCMDriver::Shutdown();

  // Dispose the syncer in order to release the reference to
  // URLRequestContextGetter that needs to be done before IOThread gets
  // deleted.
  gcm_channel_status_syncer_.reset();

  io_thread_->DeleteSoon(FROM_HERE, io_worker_.release());
}

void GCMDriverDesktop::OnSignedIn() {
  signed_in_ = true;
}

void GCMDriverDesktop::OnSignedOut() {
  signed_in_ = false;
}

void GCMDriverDesktop::AddAppHandler(const std::string& app_id,
                                     GCMAppHandler* handler) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
  GCMDriver::AddAppHandler(app_id, handler);

   // Ensures that the GCM service is started when there is an interest.
  EnsureStarted(GCMClient::DELAYED_START);
}

void GCMDriverDesktop::RemoveAppHandler(const std::string& app_id) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
  GCMDriver::RemoveAppHandler(app_id);

  // Stops the GCM service when no app intends to consume it. Stop function will
  // remove the last app handler - account mapper.
  if (app_handlers().size() == 1) {
    Stop();
    gcm_channel_status_syncer_->Stop();
  }
}

void GCMDriverDesktop::AddConnectionObserver(GCMConnectionObserver* observer) {
  connection_observer_list_.AddObserver(observer);
}

void GCMDriverDesktop::RemoveConnectionObserver(
    GCMConnectionObserver* observer) {
  connection_observer_list_.RemoveObserver(observer);
}

void GCMDriverDesktop::Enable() {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  if (gcm_enabled_)
    return;
  gcm_enabled_ = true;

  EnsureStarted(GCMClient::DELAYED_START);
}

void GCMDriverDesktop::Disable() {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  if (!gcm_enabled_)
    return;
  gcm_enabled_ = false;

  Stop();
}

void GCMDriverDesktop::Stop() {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  // No need to stop GCM service if not started yet.
  if (!gcm_started_)
    return;

  account_mapper_->ShutdownHandler();
  GCMDriver::RemoveAppHandler(kGCMAccountMapperAppId);

  RemoveCachedData();

  io_thread_->PostTask(FROM_HERE,
                       base::BindOnce(&GCMDriverDesktop::IOWorker::Stop,
                                      base::Unretained(io_worker_.get())));
}

void GCMDriverDesktop::RegisterImpl(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  // Delay the register operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(base::Bind(&GCMDriverDesktop::DoRegister,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 app_id,
                                                 sender_ids));
    return;
  }

  DoRegister(app_id, sender_ids);
}

void GCMDriverDesktop::DoRegister(const std::string& app_id,
                                  const std::vector<std::string>& sender_ids) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
  if (!HasRegisterCallback(app_id)) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::Register,
                     base::Unretained(io_worker_.get()), app_id, sender_ids));
}

void GCMDriverDesktop::UnregisterImpl(const std::string& app_id) {
  // Delay the unregister operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::DoUnregister,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id));
    return;
  }

  DoUnregister(app_id);
}

void GCMDriverDesktop::DoUnregister(const std::string& app_id) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  // Ask the server to unregister it. There could be a small chance that the
  // unregister request fails. If this occurs, it does not bring any harm since
  // we simply reject the messages/events received from the server.
  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::IOWorker::Unregister,
                                base::Unretained(io_worker_.get()), app_id));
}

void GCMDriverDesktop::SendImpl(const std::string& app_id,
                                const std::string& receiver_id,
                                const OutgoingMessage& message) {
  // Delay the send operation until all GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(base::Bind(&GCMDriverDesktop::DoSend,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 app_id,
                                                 receiver_id,
                                                 message));
    return;
  }

  DoSend(app_id, receiver_id, message);
}

void GCMDriverDesktop::DoSend(const std::string& app_id,
                              const std::string& receiver_id,
                              const OutgoingMessage& message) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::IOWorker::Send,
                                base::Unretained(io_worker_.get()), app_id,
                                receiver_id, message));
}

void GCMDriverDesktop::RecordDecryptionFailure(const std::string& app_id,
                                               GCMDecryptionResult result) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::RecordDecryptionFailure,
                     base::Unretained(io_worker_.get()), app_id, result));
}

GCMClient* GCMDriverDesktop::GetGCMClientForTesting() const {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
  return io_worker_ ? io_worker_->gcm_client_for_testing() : nullptr;
}

bool GCMDriverDesktop::IsStarted() const {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
  return gcm_started_;
}

bool GCMDriverDesktop::IsConnected() const {
  return connected_;
}

void GCMDriverDesktop::GetGCMStatistics(
    const GetGCMStatisticsCallback& callback,
    ClearActivityLogs clear_logs) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());

  request_gcm_statistics_callback_ = callback;
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::GetGCMStatistics,
                     base::Unretained(io_worker_.get()), clear_logs));
}

void GCMDriverDesktop::SetGCMRecording(const GetGCMStatisticsCallback& callback,
                                       bool recording) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  request_gcm_statistics_callback_ = callback;
  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::IOWorker::SetGCMRecording,
                                base::Unretained(io_worker_.get()), recording));
}

void GCMDriverDesktop::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::UpdateAccountMapping,
                     base::Unretained(io_worker_.get()), account_mapping));
}

void GCMDriverDesktop::RemoveAccountMapping(const std::string& account_id) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::RemoveAccountMapping,
                     base::Unretained(io_worker_.get()), account_id));
}

base::Time GCMDriverDesktop::GetLastTokenFetchTime() {
  return last_token_fetch_time_;
}

void GCMDriverDesktop::SetLastTokenFetchTime(const base::Time& time) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  last_token_fetch_time_ = time;

  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::SetLastTokenFetchTime,
                     base::Unretained(io_worker_.get()), time));
}

InstanceIDHandler* GCMDriverDesktop::GetInstanceIDHandlerInternal() {
  return this;
}

void GCMDriverDesktop::GetToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope,
    const std::map<std::string, std::string>& options,
    const GetTokenCallback& callback) {
  DCHECK(!app_id.empty());
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());
  DCHECK(!callback.is_null());
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    callback.Run(std::string(), result);
    return;
  }

  // If previous GetToken operation is still in progress, bail out.
  TokenTuple tuple_key(app_id, authorized_entity, scope);
  if (get_token_callbacks_.find(tuple_key) != get_token_callbacks_.end()) {
    callback.Run(std::string(), GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  get_token_callbacks_[tuple_key] = callback;

  // Delay the GetToken operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::DoGetToken,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id,
                   authorized_entity,
                   scope,
                   options));
    return;
  }

  DoGetToken(app_id, authorized_entity, scope, options);
}

void GCMDriverDesktop::DoGetToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope,
    const std::map<std::string, std::string>& options) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  TokenTuple tuple_key(app_id, authorized_entity, scope);
  auto callback_iter = get_token_callbacks_.find(tuple_key);
  if (callback_iter == get_token_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::IOWorker::GetToken,
                                base::Unretained(io_worker_.get()), app_id,
                                authorized_entity, scope, options));
}

void GCMDriverDesktop::ValidateToken(const std::string& app_id,
                                     const std::string& authorized_entity,
                                     const std::string& scope,
                                     const std::string& token,
                                     const ValidateTokenCallback& callback) {
  DCHECK(!app_id.empty());
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());
  DCHECK(!token.empty());
  DCHECK(!callback.is_null());
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    // Can't tell whether the registration is valid or not, so don't run the
    // callback (let it hang indefinitely).
    return;
  }

  // Only validating current state, so ignore pending get_token_callbacks_.

  auto instance_id_info = std::make_unique<InstanceIDTokenInfo>();
  instance_id_info->app_id = app_id;
  instance_id_info->authorized_entity = authorized_entity;
  instance_id_info->scope = scope;

  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::DoValidateRegistration,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&instance_id_info), token, callback));
    return;
  }

  DoValidateRegistration(std::move(instance_id_info), token, callback);
}

void GCMDriverDesktop::DeleteToken(const std::string& app_id,
                                   const std::string& authorized_entity,
                                   const std::string& scope,
                                   const DeleteTokenCallback& callback) {
  DCHECK(!app_id.empty());
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());
  DCHECK(!callback.is_null());
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    callback.Run(result);
    return;
  }

  // If previous GetToken operation is still in progress, bail out.
  TokenTuple tuple_key(app_id, authorized_entity, scope);
  if (delete_token_callbacks_.find(tuple_key) !=
      delete_token_callbacks_.end()) {
    callback.Run(GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  delete_token_callbacks_[tuple_key] = callback;

  // Delay the DeleteToken operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::DoDeleteToken,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id,
                   authorized_entity,
                   scope));
    return;
  }

  DoDeleteToken(app_id, authorized_entity, scope);
}

void GCMDriverDesktop::DoDeleteToken(const std::string& app_id,
                                     const std::string& authorized_entity,
                                     const std::string& scope) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::IOWorker::DeleteToken,
                                base::Unretained(io_worker_.get()), app_id,
                                authorized_entity, scope));
}

void GCMDriverDesktop::AddInstanceIDData(
    const std::string& app_id,
    const std::string& instance_id,
    const std::string& extra_data) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS)
    return;

  // Delay the operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::DoAddInstanceIDData,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id,
                   instance_id,
                   extra_data));
    return;
  }

  DoAddInstanceIDData(app_id, instance_id, extra_data);
}

void GCMDriverDesktop::DoAddInstanceIDData(
    const std::string& app_id,
    const std::string& instance_id,
    const std::string& extra_data) {
  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::IOWorker::AddInstanceIDData,
                                base::Unretained(io_worker_.get()), app_id,
                                instance_id, extra_data));
}

void GCMDriverDesktop::RemoveInstanceIDData(const std::string& app_id) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS)
    return;

  // Delay the operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::DoRemoveInstanceIDData,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id));
    return;
  }

  DoRemoveInstanceIDData(app_id);
}

void GCMDriverDesktop::DoRemoveInstanceIDData(const std::string& app_id) {
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::RemoveInstanceIDData,
                     base::Unretained(io_worker_.get()), app_id));
}

void GCMDriverDesktop::GetInstanceIDData(
    const std::string& app_id,
    const GetInstanceIDDataCallback& callback) {
  DCHECK(!get_instance_id_data_callbacks_.count(app_id));
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, std::string(), std::string()));
    return;
  }

  get_instance_id_data_callbacks_[app_id] = callback;

  // Delay the operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::DoGetInstanceIDData,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id));
    return;
  }

  DoGetInstanceIDData(app_id);
}

void GCMDriverDesktop::DoGetInstanceIDData(const std::string& app_id) {
  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::IOWorker::GetInstanceIDData,
                                base::Unretained(io_worker_.get()), app_id));
}

void GCMDriverDesktop::GetInstanceIDDataFinished(
    const std::string& app_id,
    const std::string& instance_id,
    const std::string& extra_data) {
  DCHECK(get_instance_id_data_callbacks_.count(app_id));
  get_instance_id_data_callbacks_[app_id].Run(instance_id, extra_data);
  get_instance_id_data_callbacks_.erase(app_id);
}

void GCMDriverDesktop::GetTokenFinished(const std::string& app_id,
                                        const std::string& authorized_entity,
                                        const std::string& scope,
                                        const std::string& token,
                                        GCMClient::Result result) {
  TokenTuple tuple_key(app_id, authorized_entity, scope);
  auto callback_iter = get_token_callbacks_.find(tuple_key);
  if (callback_iter == get_token_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  GetTokenCallback callback = callback_iter->second;
  get_token_callbacks_.erase(callback_iter);
  callback.Run(token, result);
}

void GCMDriverDesktop::DeleteTokenFinished(const std::string& app_id,
                                           const std::string& authorized_entity,
                                           const std::string& scope,
                                           GCMClient::Result result) {
  TokenTuple tuple_key(app_id, authorized_entity, scope);
  auto callback_iter = delete_token_callbacks_.find(tuple_key);
  if (callback_iter == delete_token_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  DeleteTokenCallback callback = callback_iter->second;
  delete_token_callbacks_.erase(callback_iter);
  callback.Run(result);
}

void GCMDriverDesktop::WakeFromSuspendForHeartbeat(bool wake) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  wake_from_suspend_enabled_ = wake;

  // The GCM service has not been initialized.
  if (!delayed_task_controller_)
    return;

  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    // The GCM service was initialized but has not started yet.
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::WakeFromSuspendForHeartbeat,
                   weak_ptr_factory_.GetWeakPtr(),
                   wake_from_suspend_enabled_));
    return;
  }

  // The GCMClient is ready so we can go ahead and post this task to the
  // IOWorker.
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::WakeFromSuspendForHeartbeat,
                     base::Unretained(io_worker_.get()),
                     wake_from_suspend_enabled_));
}

void GCMDriverDesktop::AddHeartbeatInterval(const std::string& scope,
                                            int interval_ms) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  // The GCM service has not been initialized.
  if (!delayed_task_controller_)
    return;

  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    // The GCM service was initialized but has not started yet.
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::AddHeartbeatInterval,
                   weak_ptr_factory_.GetWeakPtr(), scope, interval_ms));
    return;
  }

  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::AddHeartbeatInterval,
                     base::Unretained(io_worker_.get()), scope, interval_ms));
}

void GCMDriverDesktop::RemoveHeartbeatInterval(const std::string& scope) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  // The GCM service has not been initialized.
  if (!delayed_task_controller_)
    return;

  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    // The GCM service was initialized but has not started yet.
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::RemoveHeartbeatInterval,
                   weak_ptr_factory_.GetWeakPtr(), scope));
    return;
  }

  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::RemoveHeartbeatInterval,
                     base::Unretained(io_worker_.get()), scope));
}

void GCMDriverDesktop::SetAccountTokens(
    const std::vector<GCMClient::AccountTokenInfo>& account_tokens) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  account_mapper_->SetAccountTokens(account_tokens);

  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::SetAccountTokens,
                     base::Unretained(io_worker_.get()), account_tokens));
}

GCMClient::Result GCMDriverDesktop::EnsureStarted(
    GCMClient::StartMode start_mode) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  if (gcm_started_)
    return GCMClient::SUCCESS;

  // Have any app requested the service?
  if (app_handlers().empty())
    return GCMClient::UNKNOWN_ERROR;

  // Polling for channel status should be invoked when GCM is being requested,
  // no matter whether GCM is enabled or nor.
  gcm_channel_status_syncer_->EnsureStarted();

  if (!gcm_enabled_)
    return GCMClient::GCM_DISABLED;

  if (!delayed_task_controller_)
    delayed_task_controller_.reset(new GCMDelayedTaskController);

  // Note that we need to pass weak pointer again since the existing weak
  // pointer in IOWorker might have been invalidated when GCM is stopped.
  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::IOWorker::Start,
                                base::Unretained(io_worker_.get()), start_mode,
                                weak_ptr_factory_.GetWeakPtr()));

  return GCMClient::SUCCESS;
}

void GCMDriverDesktop::RemoveCachedData() {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
  // Remove all the queued tasks since they no longer make sense after
  // GCM service is stopped.
  weak_ptr_factory_.InvalidateWeakPtrs();

  gcm_started_ = false;
  delayed_task_controller_.reset();
  ClearCallbacks();
}

void GCMDriverDesktop::MessageReceived(const std::string& app_id,
                                       const IncomingMessage& message) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  // Drop the event if the service has been stopped.
  if (!gcm_started_)
    return;

  DispatchMessage(app_id, message);
}

void GCMDriverDesktop::MessagesDeleted(const std::string& app_id) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  // Drop the event if the service has been stopped.
  if (!gcm_started_)
    return;

  GCMAppHandler* handler = GetAppHandler(app_id);
  if (handler)
    handler->OnMessagesDeleted(app_id);
}

void GCMDriverDesktop::MessageSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  // Drop the event if the service has been stopped.
  if (!gcm_started_)
    return;

  GCMAppHandler* handler = GetAppHandler(app_id);
  if (handler)
    handler->OnSendError(app_id, send_error_details);
}

void GCMDriverDesktop::SendAcknowledged(const std::string& app_id,
                                        const std::string& message_id) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  // Drop the event if the service has been stopped.
  if (!gcm_started_)
    return;

  GCMAppHandler* handler = GetAppHandler(app_id);
  if (handler)
    handler->OnSendAcknowledged(app_id, message_id);
}

void GCMDriverDesktop::GCMClientReady(
    const std::vector<AccountMapping>& account_mappings,
    const base::Time& last_token_fetch_time) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  UMA_HISTOGRAM_BOOLEAN("GCM.UserSignedIn", signed_in_);

  gcm_started_ = true;
  if (wake_from_suspend_enabled_)
    WakeFromSuspendForHeartbeat(wake_from_suspend_enabled_);

  last_token_fetch_time_ = last_token_fetch_time;

  GCMDriver::AddAppHandler(kGCMAccountMapperAppId, account_mapper_.get());
  account_mapper_->Initialize(account_mappings,
                              base::Bind(&GCMDriverDesktop::MessageReceived,
                                         weak_ptr_factory_.GetWeakPtr()));

  delayed_task_controller_->SetReady();
}

void GCMDriverDesktop::OnConnected(const net::IPEndPoint& ip_endpoint) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  connected_ = true;

  // Drop the event if the service has been stopped.
  if (!gcm_started_)
    return;

  for (GCMConnectionObserver& observer : connection_observer_list_)
    observer.OnConnected(ip_endpoint);
}

void GCMDriverDesktop::OnDisconnected() {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  connected_ = false;

  // Drop the event if the service has been stopped.
  if (!gcm_started_)
    return;

  for (GCMConnectionObserver& observer : connection_observer_list_)
    observer.OnDisconnected();
}

void GCMDriverDesktop::OnStoreReset() {
  // Defensive copy in case OnStoreReset calls Add/RemoveAppHandler.
  std::vector<GCMAppHandler*> app_handler_values;
  for (const auto& key_value : app_handlers())
    app_handler_values.push_back(key_value.second);
  for (GCMAppHandler* app_handler : app_handler_values) {
    app_handler->OnStoreReset();
    // app_handler might now have been deleted.
  }
}

void GCMDriverDesktop::GetGCMStatisticsFinished(
    const GCMClient::GCMStatistics& stats) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  // request_gcm_statistics_callback_ could be null when an activity, i.e.
  // network activity, is triggered while gcm-intenals page is not open.
  if (!request_gcm_statistics_callback_.is_null())
    request_gcm_statistics_callback_.Run(stats);
}

bool GCMDriverDesktop::TokenTupleComparer::operator()(
    const TokenTuple& a, const TokenTuple& b) const {
  if (std::get<0>(a) < std::get<0>(b))
    return true;
  if (std::get<0>(a) > std::get<0>(b))
    return false;

  if (std::get<1>(a) < std::get<1>(b))
    return true;
  if (std::get<1>(a) > std::get<1>(b))
    return false;

  return std::get<2>(a) < std::get<2>(b);
}

}  // namespace gcm

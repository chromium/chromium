// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_desktop.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "components/gcm_driver/gcm_account_mapper.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_delayed_task_controller.h"
#include "components/gcm_driver/instance_id/instance_id_impl.h"
#include "components/gcm_driver/system_encryptor.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace gcm {

class GCMDriverDesktop::IOWorker : public GCMClient::Delegate {
 public:
  // Called on UI thread.
  IOWorker(const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
           const scoped_refptr<base::SequencedTaskRunner>& io_thread);

  IOWorker(const IOWorker&) = delete;
  IOWorker& operator=(const IOWorker&) = delete;

  virtual ~IOWorker();

  // Overridden from GCMClient::Delegate:
  // Called on IO thread.
  void OnRegisterFinished(scoped_refptr<RegistrationInfo> registration_info,
                          const std::string& registration_id,
                          GCMClient::Result result) override;
  void OnUnregisterFinished(scoped_refptr<RegistrationInfo> registration_info,
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
      base::RepeatingCallback<void(
          mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
          get_socket_factory_callback,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
  void Start(GCMClient::StartMode start_mode,
             const base::WeakPtr<GCMDriverDesktop>& service,
             base::TimeTicks time_task_posted);
  void Stop();
  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids);
  void Unregister(const std::string& app_id);
  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const OutgoingMessage& message);
  void GetGCMStatistics(GetGCMStatisticsCallback callback,
                        GCMDriver::ClearActivityLogs clear_logs);
  void SetGCMRecording(GetGCMStatisticsCallback callback, bool recording);

  void SetAccountTokens(
      const std::vector<GCMClient::AccountTokenInfo>& account_tokens);
  void UpdateAccountMapping(const AccountMapping& account_mapping);
  void RemoveAccountMapping(const CoreAccountId& account_id);
  void SetLastTokenFetchTime(const base::Time& time);
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
                base::TimeDelta time_to_live);
  bool ValidateRegistration(scoped_refptr<RegistrationInfo> registration_info,
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
};

GCMDriverDesktop::IOWorker::IOWorker(
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& io_thread)
    : ui_thread_(ui_thread), io_thread_(io_thread) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
}

GCMDriverDesktop::IOWorker::~IOWorker() {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
}

void GCMDriverDesktop::IOWorker::Initialize(
    std::unique_ptr<GCMClientFactory> gcm_client_factory,
    const GCMClient::ChromeBuildInfo& chrome_build_info,
    const base::FilePath& store_path,
    base::RepeatingCallback<void(
        mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
        get_socket_factory_callback,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  gcm_client_ = gcm_client_factory->BuildInstance();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_for_io =
      network::SharedURLLoaderFactory::Create(
          std::move(pending_loader_factory));

  gcm_client_->Initialize(chrome_build_info, store_path, blocking_task_runner,
                          io_thread_, std::move(get_socket_factory_callback),
                          url_loader_factory_for_io, network_connection_tracker,
                          std::make_unique<SystemEncryptor>(), this);
}

void GCMDriverDesktop::IOWorker::OnRegisterFinished(
    scoped_refptr<RegistrationInfo> registration_info,
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
    scoped_refptr<RegistrationInfo> registration_info,
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
  gcm::GCMClient::GCMStatistics stats;
  if (gcm_client_) {
    stats = gcm_client_->GetStatistics();
  }
  ui_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::OnActivityRecorded, service_, stats));
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
    const base::WeakPtr<GCMDriverDesktop>& service,
    base::TimeTicks time_task_posted) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  // Record for how long current task has been delayed. This is important during
  // the browser startup when some best effort tasks are postponed.
  base::UmaHistogramMediumTimes("GCM.ClientStartDelay",
                                base::TimeTicks::Now() - time_task_posted);

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

  auto gcm_info = base::MakeRefCounted<GCMRegistrationInfo>();
  gcm_info->app_id = app_id;
  gcm_info->sender_ids = sender_ids;
  gcm_client_->Register(std::move(gcm_info));
}

bool GCMDriverDesktop::IOWorker::ValidateRegistration(
    scoped_refptr<RegistrationInfo> registration_info,
    const std::string& registration_id) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  return gcm_client_->ValidateRegistration(std::move(registration_info),
                                           registration_id);
}

void GCMDriverDesktop::IOWorker::Unregister(const std::string& app_id) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  auto gcm_info = base::MakeRefCounted<GCMRegistrationInfo>();
  gcm_info->app_id = app_id;
  gcm_client_->Unregister(std::move(gcm_info));
}

void GCMDriverDesktop::IOWorker::Send(const std::string& app_id,
                                      const std::string& receiver_id,
                                      const OutgoingMessage& message) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  gcm_client_->Send(app_id, receiver_id, message);
}

void GCMDriverDesktop::IOWorker::GetGCMStatistics(
    GetGCMStatisticsCallback callback,
    ClearActivityLogs clear_logs) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
  gcm::GCMClient::GCMStatistics stats;

  if (gcm_client_) {
    if (clear_logs == GCMDriver::CLEAR_LOGS)
      gcm_client_->ClearActivityLogs();
    stats = gcm_client_->GetStatistics();
  }

  ui_thread_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), stats));
}

void GCMDriverDesktop::IOWorker::SetGCMRecording(
    GetGCMStatisticsCallback callback,
    bool recording) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
  gcm::GCMClient::GCMStatistics stats;

  if (gcm_client_) {
    gcm_client_->SetRecording(recording);
    stats = gcm_client_->GetStatistics();
    stats.gcm_client_created = true;
  }

  ui_thread_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), stats));
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
    const CoreAccountId& account_id) {
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

void GCMDriverDesktop::IOWorker::GetInstanceIDData(const std::string& app_id) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  std::string instance_id;
  std::string extra_data;
  if (gcm_client_)
    gcm_client_->GetInstanceIDData(app_id, &instance_id, &extra_data);

  ui_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::GetInstanceIDDataFinished,
                                service_, app_id, instance_id, extra_data));
}

void GCMDriverDesktop::IOWorker::GetToken(const std::string& app_id,
                                          const std::string& authorized_entity,
                                          const std::string& scope,
                                          base::TimeDelta time_to_live) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  auto instance_id_token_info = base::MakeRefCounted<InstanceIDTokenInfo>();
  instance_id_token_info->app_id = app_id;
  instance_id_token_info->authorized_entity = authorized_entity;
  instance_id_token_info->scope = scope;
  instance_id_token_info->time_to_live = time_to_live;
  gcm_client_->Register(std::move(instance_id_token_info));
}

void GCMDriverDesktop::IOWorker::DeleteToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope) {
  auto instance_id_token_info = base::MakeRefCounted<InstanceIDTokenInfo>();
  instance_id_token_info->app_id = app_id;
  instance_id_token_info->authorized_entity = authorized_entity;
  instance_id_token_info->scope = scope;
  gcm_client_->Unregister(std::move(instance_id_token_info));
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
    PrefService* prefs,
    const base::FilePath& store_path,
    base::RepeatingCallback<void(
        mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
        get_socket_factory_callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_for_ui,
    network::NetworkConnectionTracker* network_connection_tracker,
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& io_thread,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : GCMDriver(store_path, blocking_task_runner),
      gcm_started_(false),
      connected_(false),
      account_mapper_(new GCMAccountMapper(this)),
      // Setting to max, to make sure it does not prompt for token reporting
      // Before reading a reasonable value from the DB, which might be never,
      // in which case the fetching will be triggered.
      last_token_fetch_time_(base::Time::Max()),
      ui_thread_(ui_thread),
      io_thread_(io_thread) {
  // Create and initialize the GCMClient. Note that this does not initiate the
  // GCM check-in.
  io_worker_ = std::make_unique<IOWorker>(ui_thread, io_thread);
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

GCMDriverDesktop::~GCMDriverDesktop() = default;

void GCMDriverDesktop::ValidateRegistration(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const std::string& registration_id,
    ValidateRegistrationCallback callback) {
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

  auto gcm_info = base::MakeRefCounted<GCMRegistrationInfo>();
  gcm_info->app_id = app_id;
  gcm_info->sender_ids = sender_ids;
  // Normalize the sender IDs by making them sorted.
  std::sort(gcm_info->sender_ids.begin(), gcm_info->sender_ids.end());

  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::BindOnce(&GCMDriverDesktop::DoValidateRegistration,
                       weak_ptr_factory_.GetWeakPtr(), gcm_info,
                       registration_id, std::move(callback)));
    return;
  }

  DoValidateRegistration(std::move(gcm_info), registration_id,
                         std::move(callback));
}

void GCMDriverDesktop::DoValidateRegistration(
    scoped_refptr<RegistrationInfo> registration_info,
    const std::string& registration_id,
    ValidateRegistrationCallback callback) {
  io_thread_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::ValidateRegistration,
                     base::Unretained(io_worker_.get()),
                     std::move(registration_info), registration_id),
      std::move(callback));
}

void GCMDriverDesktop::Shutdown() {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  Stop();
  GCMDriver::Shutdown();

  io_thread_->DeleteSoon(FROM_HERE, io_worker_.release());
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
    DVLOG(1) << "Removed last app handler, calling GCMDriverDesktop::Stop now.";
    Stop();
  }
}

void GCMDriverDesktop::AddConnectionObserver(GCMConnectionObserver* observer) {
  connection_observer_list_.AddObserver(observer);
}

void GCMDriverDesktop::RemoveConnectionObserver(
    GCMConnectionObserver* observer) {
  connection_observer_list_.RemoveObserver(observer);
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
    delayed_task_controller_->AddTask(
        base::BindOnce(&GCMDriverDesktop::DoRegister,
                       weak_ptr_factory_.GetWeakPtr(), app_id, sender_ids));
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
        base::BindOnce(&GCMDriverDesktop::DoUnregister,
                       weak_ptr_factory_.GetWeakPtr(), app_id));
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
    delayed_task_controller_->AddTask(base::BindOnce(
        &GCMDriverDesktop::DoSend, weak_ptr_factory_.GetWeakPtr(), app_id,
        receiver_id, message));
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

void GCMDriverDesktop::GetGCMStatistics(GetGCMStatisticsCallback callback,
                                        ClearActivityLogs clear_logs) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());

  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::IOWorker::GetGCMStatistics,
                                base::Unretained(io_worker_.get()),
                                std::move(callback), clear_logs));
}

void GCMDriverDesktop::SetGCMRecording(
    const GCMStatisticsRecordingCallback& callback,
    bool recording) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  gcm_statistics_recording_callback_ = callback;
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::SetGCMRecording,
                     base::Unretained(io_worker_.get()), callback, recording));
}

void GCMDriverDesktop::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMDriverDesktop::IOWorker::UpdateAccountMapping,
                     base::Unretained(io_worker_.get()), account_mapping));
}

void GCMDriverDesktop::RemoveAccountMapping(const CoreAccountId& account_id) {
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

void GCMDriverDesktop::GetToken(const std::string& app_id,
                                const std::string& authorized_entity,
                                const std::string& scope,
                                base::TimeDelta time_to_live,
                                GetTokenCallback callback) {
  DCHECK(!app_id.empty());
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());
  DCHECK(!callback.is_null());
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    DLOG(ERROR)
        << "Unable to get the InstanceID token: cannot start the GCM Client";

    std::move(callback).Run(std::string(), result);
    return;
  }

  // If previous GetToken operation is still in progress, bail out.
  TokenTuple tuple_key(app_id, authorized_entity, scope);
  if (get_token_callbacks_.find(tuple_key) != get_token_callbacks_.end()) {
    std::move(callback).Run(std::string(), GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  get_token_callbacks_[tuple_key] = std::move(callback);

  // Delay the GetToken operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(base::BindOnce(
        &GCMDriverDesktop::DoGetToken, weak_ptr_factory_.GetWeakPtr(), app_id,
        authorized_entity, scope, time_to_live));
    return;
  }

  DoGetToken(app_id, authorized_entity, scope, time_to_live);
}

void GCMDriverDesktop::DoGetToken(const std::string& app_id,
                                  const std::string& authorized_entity,
                                  const std::string& scope,
                                  base::TimeDelta time_to_live) {
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
                                authorized_entity, scope, time_to_live));
}

void GCMDriverDesktop::ValidateToken(const std::string& app_id,
                                     const std::string& authorized_entity,
                                     const std::string& scope,
                                     const std::string& token,
                                     ValidateTokenCallback callback) {
  DCHECK(!app_id.empty());
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());
  DCHECK(!token.empty());
  DCHECK(callback);
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    // Can't tell whether the registration is valid or not, so don't run the
    // callback (let it hang indefinitely).
    DLOG(ERROR) << "Unable to validate the InstanceID token: cannot start the "
                   "GCM Client";
    return;
  }

  // Only validating current state, so ignore pending get_token_callbacks_.

  auto instance_id_info = base::MakeRefCounted<InstanceIDTokenInfo>();
  instance_id_info->app_id = app_id;
  instance_id_info->authorized_entity = authorized_entity;
  instance_id_info->scope = scope;

  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::BindOnce(&GCMDriverDesktop::DoValidateRegistration,
                       weak_ptr_factory_.GetWeakPtr(), instance_id_info, token,
                       std::move(callback)));
    return;
  }

  DoValidateRegistration(std::move(instance_id_info), token,
                         std::move(callback));
}

void GCMDriverDesktop::DeleteToken(const std::string& app_id,
                                   const std::string& authorized_entity,
                                   const std::string& scope,
                                   DeleteTokenCallback callback) {
  DCHECK(!app_id.empty());
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());
  DCHECK(!callback.is_null());
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    DLOG(ERROR)
        << "Unable to delete the InstanceID token: cannot start the GCM Client";

    std::move(callback).Run(result);
    return;
  }

  // If previous GetToken operation is still in progress, bail out.
  TokenTuple tuple_key(app_id, authorized_entity, scope);
  if (delete_token_callbacks_.find(tuple_key) !=
      delete_token_callbacks_.end()) {
    std::move(callback).Run(GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  delete_token_callbacks_[tuple_key] = std::move(callback);

  // Delay the DeleteToken operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(base::BindOnce(
        &GCMDriverDesktop::DoDeleteToken, weak_ptr_factory_.GetWeakPtr(),
        app_id, authorized_entity, scope));
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

void GCMDriverDesktop::AddInstanceIDData(const std::string& app_id,
                                         const std::string& instance_id,
                                         const std::string& extra_data) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  if (result != GCMClient::SUCCESS) {
    DLOG(ERROR)
        << "Unable to add the InstanceID data: cannot start the GCM Client";
    return;
  }

  // Delay the operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(base::BindOnce(
        &GCMDriverDesktop::DoAddInstanceIDData, weak_ptr_factory_.GetWeakPtr(),
        app_id, instance_id, extra_data));
    return;
  }

  DoAddInstanceIDData(app_id, instance_id, extra_data);
}

void GCMDriverDesktop::DoAddInstanceIDData(const std::string& app_id,
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
  if (result != GCMClient::SUCCESS) {
    DLOG(ERROR)
        << "Unable to remove the InstanceID data: cannot start the GCM Client";
    return;
  }

  // Delay the operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::BindOnce(&GCMDriverDesktop::DoRemoveInstanceIDData,
                       weak_ptr_factory_.GetWeakPtr(), app_id));
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

void GCMDriverDesktop::GetInstanceIDData(const std::string& app_id,
                                         GetInstanceIDDataCallback callback) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  GCMClient::Result result = EnsureStarted(GCMClient::IMMEDIATE_START);
  // TODO(crbug.com/40109289): This method is only used by InstanceIDImpl to get
  // the current instance ID from the store. As this method doesn't support
  // error codes, the instance ID will assume no current ID and generate a new
  // one if the gcm client is not ready and we pass an empty string to the
  // callback below. We should fix this!
  if (result != GCMClient::SUCCESS) {
    DLOG(ERROR)
        << "Unable to get the InstanceID data: cannot start the GCM Client";
    // Resolve the |callback| to not leave it hanging indefinitely.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::string(), std::string()));
    return;
  }

  get_instance_id_data_callbacks_[app_id].push(std::move(callback));

  // Delay the operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::BindOnce(&GCMDriverDesktop::DoGetInstanceIDData,
                       weak_ptr_factory_.GetWeakPtr(), app_id));
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
  auto iter = get_instance_id_data_callbacks_.find(app_id);
  CHECK(iter != get_instance_id_data_callbacks_.end(),
        base::NotFatalUntil::M130);

  base::queue<GetInstanceIDDataCallback>& callbacks = iter->second;
  std::move(callbacks.front()).Run(instance_id, extra_data);

  callbacks.pop();

  if (!callbacks.size())
    get_instance_id_data_callbacks_.erase(iter);
}

void GCMDriverDesktop::GetTokenFinished(const std::string& app_id,
                                        const std::string& authorized_entity,
                                        const std::string& scope,
                                        const std::string& token,
                                        GCMClient::Result result) {
  TRACE_EVENT0("identity", "GCMDriverDesktop::GetTokenFinished");
  TokenTuple tuple_key(app_id, authorized_entity, scope);
  auto callback_iter = get_token_callbacks_.find(tuple_key);
  if (callback_iter == get_token_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  GetTokenCallback callback = std::move(callback_iter->second);
  get_token_callbacks_.erase(callback_iter);
  std::move(callback).Run(token, result);
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

  DeleteTokenCallback callback = std::move(callback_iter->second);
  delete_token_callbacks_.erase(callback_iter);
  std::move(callback).Run(result);
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
        base::BindOnce(&GCMDriverDesktop::AddHeartbeatInterval,
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
        base::BindOnce(&GCMDriverDesktop::RemoveHeartbeatInterval,
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

  if (!delayed_task_controller_)
    delayed_task_controller_ = std::make_unique<GCMDelayedTaskController>();

  // Note that we need to pass weak pointer again since the existing weak
  // pointer in IOWorker might have been invalidated when GCM is stopped.
  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&GCMDriverDesktop::IOWorker::Start,
                                base::Unretained(io_worker_.get()), start_mode,
                                weak_ptr_factory_.GetWeakPtr(),
                                /*time_task_posted=*/base::TimeTicks::Now()));

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

  gcm_started_ = true;

  last_token_fetch_time_ = last_token_fetch_time;

  GCMDriver::AddAppHandler(kGCMAccountMapperAppId, account_mapper_.get());
  account_mapper_->Initialize(
      account_mappings, base::BindRepeating(&GCMDriverDesktop::MessageReceived,
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

void GCMDriverDesktop::OnActivityRecorded(
    const GCMClient::GCMStatistics& stats) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  if (gcm_statistics_recording_callback_)
    gcm_statistics_recording_callback_.Run(stats);
}

bool GCMDriverDesktop::TokenTupleComparer::operator()(
    const TokenTuple& a,
    const TokenTuple& b) const {
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

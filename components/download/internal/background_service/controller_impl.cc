// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/controller_impl.h"

#include <inttypes.h>

#include <algorithm>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/download/internal/background_service/client_set.h"
#include "components/download/internal/background_service/config.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/entry_utils.h"
#include "components/download/internal/background_service/file_monitor.h"
#include "components/download/internal/background_service/log_sink.h"
#include "components/download/internal/background_service/logger_impl.h"
#include "components/download/internal/background_service/model.h"
#include "components/download/internal/background_service/scheduler/scheduler.h"
#include "components/download/internal/background_service/stats.h"
#include "components/download/network/download_http_utils.h"
#include "components/download/public/background_service/client.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/navigation_monitor.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace download {
namespace {

// Helper function to transit the state of |entry| to |new_state|.
void TransitTo(Entry* entry, Entry::State new_state, Model* model) {
  DCHECK(entry);
  if (entry->state == new_state)
    return;
  entry->state = new_state;
  model->Update(*entry);
}

// Helper function to post the callback once again before starting a download.
void RunOnDownloadReadyToStart(
    GetUploadDataCallback callback,
    scoped_refptr<network::ResourceRequestBody> post_body) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), post_body));
}

// Helper function to move from a CompletionType to a Client::FailureReason.
Client::FailureReason FailureReasonFromCompletionType(CompletionType type) {
  // SUCCEED does not map to a FailureReason.
  DCHECK_NE(CompletionType::SUCCEED, type);

  switch (type) {
    case CompletionType::FAIL:            // Intentional fallthrough.
    case CompletionType::OUT_OF_RETRIES:  // Intentional fallthrough.
    case CompletionType::OUT_OF_RESUMPTIONS:
      return Client::FailureReason::NETWORK;
    case CompletionType::ABORT:
      return Client::FailureReason::ABORTED;
    case CompletionType::TIMEOUT:
      return Client::FailureReason::TIMEDOUT;
    case CompletionType::UPLOAD_TIMEOUT:
      return Client::FailureReason::UPLOAD_TIMEDOUT;
    case CompletionType::UNKNOWN:
      return Client::FailureReason::UNKNOWN;
    case CompletionType::CANCEL:
      return Client::FailureReason::CANCELLED;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return Client::FailureReason::UNKNOWN;
}

// Helper function to determine if more downloads can be activated based on
// configuration.
bool CanActivateMoreDownloads(Configuration* config,
                              uint32_t active_count,
                              uint32_t paused_count) {
  if (config->max_concurrent_downloads <= paused_count + active_count ||
      config->max_running_downloads <= active_count) {
    return false;
  }
  return true;
}

Model::EntryList GetRunnableEntries(const Model::EntryList& list) {
  Model::EntryList candidates;
  for (Entry* entry : list) {
    if (entry->state == Entry::State::AVAILABLE ||
        entry->state == Entry::State::ACTIVE) {
      candidates.emplace_back(entry);
    }
  }
  return candidates;
}

}  // namespace

ControllerImpl::ControllerImpl(
    std::unique_ptr<Configuration> config,
    std::unique_ptr<Logger> logger,
    LogSink* log_sink,
    std::unique_ptr<ClientSet> clients,
    std::unique_ptr<DownloadDriver> driver,
    std::unique_ptr<Model> model,
    std::unique_ptr<DeviceStatusListener> device_status_listener,
    NavigationMonitor* navigation_monitor,
    std::unique_ptr<Scheduler> scheduler,
    std::unique_ptr<TaskScheduler> task_scheduler,
    std::unique_ptr<FileMonitor> file_monitor,
    const base::FilePath& download_file_dir)
    : download_file_dir_(download_file_dir),
      config_(std::move(config)),
      service_config_(config_.get()),
      logger_(std::move(logger)),
      log_sink_(log_sink),
      clients_(std::move(clients)),
      driver_(std::move(driver)),
      model_(std::move(model)),
      device_status_listener_(std::move(device_status_listener)),
      navigation_monitor_(navigation_monitor),
      scheduler_(std::move(scheduler)),
      task_scheduler_(std::move(task_scheduler)),
      file_monitor_(std::move(file_monitor)),
      controller_state_(State::CREATED) {
  DCHECK(config_);
  DCHECK(log_sink_);
}

ControllerImpl::~ControllerImpl() {
  navigation_monitor_->SetObserver(nullptr);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void ControllerImpl::Initialize(base::OnceClosure callback) {
  DCHECK_EQ(controller_state_, State::CREATED);

  init_callback_ = std::move(callback);
  controller_state_ = State::INITIALIZING;

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "DownloadService",
      base::SingleThreadTaskRunner::GetCurrentDefault());

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "download_service", "DownloadServiceInitialize", TRACE_ID_LOCAL(this));

  driver_->Initialize(this);
  model_->Initialize(this);
  file_monitor_->Initialize(base::BindOnce(&ControllerImpl::OnFileMonitorReady,
                                           weak_ptr_factory_.GetWeakPtr()));
  navigation_monitor_->Configure(config_->navigation_completion_delay,
                                 config_->navigation_timeout_delay);
  navigation_monitor_->SetObserver(this);
}

const ServiceConfig& ControllerImpl::GetConfig() {
  return service_config_;
}

BackgroundDownloadService::ServiceStatus ControllerImpl::GetStatus() {
  switch (GetState()) {
    case Controller::State::CREATED:       // Intentional fallthrough.
    case Controller::State::INITIALIZING:  // Intentional fallthrough.
    case Controller::State::RECOVERING:
      return BackgroundDownloadService::ServiceStatus::STARTING_UP;
    case Controller::State::READY:
      return BackgroundDownloadService::ServiceStatus::READY;
    case Controller::State::UNAVAILABLE:  // Intentional fallthrough.
    default:
      return BackgroundDownloadService::ServiceStatus::UNAVAILABLE;
  }
}

Controller::State ControllerImpl::GetState() {
  return controller_state_;
}

void ControllerImpl::StartDownload(DownloadParams params) {
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);

  // TODO(dtrainor): Validate all input parameters.
  DCHECK_LE(base::Time::Now(), params.scheduling_params.cancel_time);
  if (!ValidateRequestHeaders(params.request_params.request_headers)) {
    HandleStartDownloadResponse(params.client, params.guid,
                                DownloadParams::StartResult::INTERNAL_ERROR,
                                std::move(params.callback));
    return;
  }

  if (controller_state_ != State::READY) {
    HandleStartDownloadResponse(params.client, params.guid,
                                DownloadParams::StartResult::INTERNAL_ERROR,
                                std::move(params.callback));
    return;
  }

  KillTimedOutDownloads();

  if (start_callbacks_.find(params.guid) != start_callbacks_.end() ||
      model_->Get(params.guid) != nullptr) {
    HandleStartDownloadResponse(params.client, params.guid,
                                DownloadParams::StartResult::UNEXPECTED_GUID,
                                std::move(params.callback));
    return;
  }

  auto* client = clients_->GetClient(params.client);
  if (!client) {
    HandleStartDownloadResponse(params.client, params.guid,
                                DownloadParams::StartResult::UNEXPECTED_CLIENT,
                                std::move(params.callback));
    return;
  }

  uint32_t client_count = util::GetNumberOfLiveEntriesForClient(
      params.client, model_->PeekEntries());
  if (client_count >= config_->max_scheduled_downloads) {
    HandleStartDownloadResponse(params.client, params.guid,
                                DownloadParams::StartResult::BACKOFF,
                                std::move(params.callback));
    return;
  }

  start_callbacks_[params.guid] = std::move(params.callback);
  Entry entry(params);
  entry.target_file_path = download_file_dir_.AppendASCII(params.guid);
  model_->Add(entry);
}

void ControllerImpl::PauseDownload(const std::string& guid) {
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);
  stats::LogServiceApiAction(GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::PAUSE_DOWNLOAD);
  if (controller_state_ != State::READY)
    return;

  auto* entry = model_->Get(guid);

  if (!entry || entry->state == Entry::State::PAUSED ||
      entry->state == Entry::State::COMPLETE ||
      entry->state == Entry::State::NEW) {
    return;
  }

  TransitTo(entry, Entry::State::PAUSED, model_.get());
  UpdateDriverState(entry);

  // Pausing a download may yield a concurrent slot to start a new download, and
  // may change the scheduling criteria.
  ActivateMoreDownloads();
}

void ControllerImpl::ResumeDownload(const std::string& guid) {
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);
  stats::LogServiceApiAction(GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::RESUME_DOWNLOAD);
  if (controller_state_ != State::READY)
    return;

  auto* entry = model_->Get(guid);
  DCHECK(entry);

  if (entry->state != Entry::State::PAUSED)
    return;

  TransitTo(entry, Entry::State::ACTIVE, model_.get());
  UpdateDriverState(entry);

  ActivateMoreDownloads();
}

void ControllerImpl::CancelDownload(const std::string& guid) {
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);
  stats::LogServiceApiAction(GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::CANCEL_DOWNLOAD);
  if (controller_state_ != State::READY)
    return;

  auto* entry = model_->Get(guid);
  if (!entry)
    return;

  if (entry->state == Entry::State::NEW) {
    // Check if we're currently trying to add the download.
    DCHECK(start_callbacks_.find(entry->guid) != start_callbacks_.end());
    HandleStartDownloadResponse(entry->client, guid,
                                DownloadParams::StartResult::CLIENT_CANCELLED);
    return;
  }

  HandleCompleteDownload(CompletionType::CANCEL, guid);
}

void ControllerImpl::ChangeDownloadCriteria(const std::string& guid,
                                            const SchedulingParams& params) {
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);
  stats::LogServiceApiAction(GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::CHANGE_CRITERIA);
  if (controller_state_ != State::READY)
    return;

  auto* entry = model_->Get(guid);
  if (!entry || entry->scheduling_params == params) {
    DVLOG(1) << "Try to update the same scheduling parameters.";
    return;
  }

  UpdateDriverState(entry);

  // Update the scheduling parameters.
  entry->scheduling_params = params;
  model_->Update(*entry);

  ActivateMoreDownloads();
}

DownloadClient ControllerImpl::GetOwnerOfDownload(const std::string& guid) {
  DCHECK(controller_state_ == State::READY ||
         controller_state_ == State::UNAVAILABLE);
  if (controller_state_ != State::READY)
    return DownloadClient::INVALID;

  auto* entry = model_->Get(guid);
  return entry ? entry->client : DownloadClient::INVALID;
}

void ControllerImpl::OnStartScheduledTask(DownloadTaskType task_type,
                                          TaskFinishedCallback callback) {
  device_status_listener_->Start(config_->network_startup_delay_backgroud_task);
  task_finished_callbacks_[task_type] = std::move(callback);

  switch (controller_state_) {
    case State::READY:
      if (task_type == DownloadTaskType::DOWNLOAD_TASK) {
        ActivateMoreDownloads();
      } else if (task_type == DownloadTaskType::CLEANUP_TASK) {
        RemoveCleanupEligibleDownloads();
        ScheduleCleanupTask();
      }
      break;
    case State::UNAVAILABLE:
    case State::CREATED:       // Intentional fallthrough.
    case State::INITIALIZING:  // Intentional fallthrough.
    case State::RECOVERING:    // Intentional fallthrough.
    default:
      HandleTaskFinished(task_type,
                         stats::ScheduledTaskStatus::ABORTED_ON_FAILED_INIT);
      break;
  }
}

bool ControllerImpl::OnStopScheduledTask(DownloadTaskType task_type) {
  HandleTaskFinished(task_type, stats::ScheduledTaskStatus::CANCELLED_ON_STOP);
  return false;
}

Logger* ControllerImpl::GetLogger() {
  return logger_.get();
}

void ControllerImpl::OnCompleteCleanupTask() {
  HandleTaskFinished(DownloadTaskType::CLEANUP_TASK,
                     stats::ScheduledTaskStatus::COMPLETED_NORMALLY);
}

void ControllerImpl::RemoveCleanupEligibleDownloads() {
  std::vector<Entry*> entries_to_remove;
  for (auto* entry : model_->PeekEntries()) {
    if (entry->state != Entry::State::COMPLETE)
      continue;

    bool optional_cleanup =
        base::Time::Now() >
        (entry->last_cleanup_check_time + config_->file_keep_alive_time);
    bool mandatory_cleanup =
        base::Time::Now() >
        (entry->completion_time + config_->max_file_keep_alive_time);

    if (!optional_cleanup && !mandatory_cleanup)
      continue;

    download::Client* client = clients_->GetClient(entry->client);
    DCHECK(client);
    bool client_ok =
        client->CanServiceRemoveDownloadedFile(entry->guid, mandatory_cleanup);

    if (client_ok || mandatory_cleanup) {
      entries_to_remove.push_back(entry);
    } else {
      entry->last_cleanup_check_time = base::Time::Now();
    }
  }

  file_monitor_->CleanupFilesForCompletedEntries(
      entries_to_remove, base::BindOnce(&ControllerImpl::OnCompleteCleanupTask,
                                        weak_ptr_factory_.GetWeakPtr()));

  for (auto* entry : entries_to_remove) {
    DCHECK_EQ(Entry::State::COMPLETE, entry->state);
    model_->Remove(entry->guid);
  }
}

void ControllerImpl::HandleTaskFinished(DownloadTaskType task_type,
                                        stats::ScheduledTaskStatus status) {
  if (task_finished_callbacks_.count(task_type) == 0)
    return;

  if (status != stats::ScheduledTaskStatus::CANCELLED_ON_STOP) {
    std::move(task_finished_callbacks_[task_type]).Run(false);
  }
  // TODO(dtrainor): It might be useful to log how many downloads we have
  // running when we're asked to stop processing.
  stats::LogScheduledTaskStatus(task_type, status);
  task_finished_callbacks_.erase(task_type);

  if (status == stats::ScheduledTaskStatus::ABORTED_ON_FAILED_INIT) {
    return;
  }

  switch (task_type) {
    case DownloadTaskType::DOWNLOAD_TASK:
      scheduler_->Reschedule(GetRunnableEntries(model_->PeekEntries()));
      break;
    case DownloadTaskType::CLEANUP_TASK:
      ScheduleCleanupTask();
      break;
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK:
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_UNMETERED_TASK:
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_TASK:
    case DownloadTaskType::DOWNLOAD_LATER_TASK:
      NOTREACHED_IN_MIGRATION();
  }
}

void ControllerImpl::OnDriverReady(bool success) {
  DCHECK(!startup_status_.driver_ok.has_value());
  startup_status_.driver_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnDriverHardRecoverComplete(bool success) {
  DCHECK(!startup_status_.driver_ok.has_value());
  startup_status_.driver_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnDownloadCreated(const DriverEntry& download) {
  if (controller_state_ != State::READY)
    return;

  Entry* entry = model_->Get(download.guid);

  if (!entry) {
    HandleExternalDownload(download.guid, true);
    return;
  }

  entry->url_chain = download.url_chain;
  entry->response_headers = download.response_headers;
  entry->did_received_response = true;
  model_->Update(*entry);

  download::Client* client = clients_->GetClient(entry->client);
  DCHECK(client);
  client->OnDownloadStarted(download.guid, download.url_chain,
                            download.response_headers);
}

void ControllerImpl::OnDownloadFailed(const DriverEntry& download,
                                      FailureType failure_type) {
  if (controller_state_ != State::READY)
    return;

  Entry* entry = model_->Get(download.guid);
  if (!entry) {
    HandleExternalDownload(download.guid, false);
    return;
  }

  if (!download.done && failure_type == FailureType::RECOVERABLE &&
      !entry->has_upload_data) {
    // Because the network offline signal comes later than actual download
    // failure, retry the download after a delay to avoid the retry to fail
    // immediately again.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ControllerImpl::UpdateDriverStateWithGuid,
                       weak_ptr_factory_.GetWeakPtr(), download.guid),
        config_->download_retry_delay);
  } else {
    HandleCompleteDownload(CompletionType::FAIL, download.guid);
  }
}

void ControllerImpl::OnDownloadSucceeded(const DriverEntry& download) {
  if (controller_state_ != State::READY)
    return;

  Entry* entry = model_->Get(download.guid);
  if (!entry) {
    HandleExternalDownload(download.guid, false);
    return;
  }

  HandleCompleteDownload(CompletionType::SUCCEED, download.guid);
}

void ControllerImpl::OnDownloadUpdated(const DriverEntry& download) {
  if (controller_state_ != State::READY)
    return;

  Entry* entry = model_->Get(download.guid);
  if (!entry) {
    HandleExternalDownload(download.guid, !download.paused);
    return;
  }

  DCHECK_EQ(download.state, DriverEntry::State::IN_PROGRESS);

  log_sink_->OnServiceDownloadChanged(entry->guid);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ControllerImpl::SendOnDownloadUpdated,
                                weak_ptr_factory_.GetWeakPtr(), entry->client,
                                download.guid, entry->bytes_uploaded,
                                download.bytes_downloaded));
}

bool ControllerImpl::IsTrackingDownload(const std::string& guid) const {
  if (controller_state_ != State::READY)
    return false;
  return !!model_->Get(guid);
}

void ControllerImpl::OnUploadProgress(const std::string& guid,
                                      uint64_t bytes_uploaded) const {
  Entry* entry = model_->Get(guid);
  DCHECK(entry);

  entry->bytes_uploaded = bytes_uploaded;

  auto* client = clients_->GetClient(entry->client);
  DCHECK(client);

  client->OnDownloadUpdated(guid, bytes_uploaded, /* bytes_downloaded= */ 0u);
}

void ControllerImpl::OnFileMonitorReady(bool success) {
  DCHECK(!startup_status_.file_monitor_ok.has_value());
  startup_status_.file_monitor_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnFileMonitorHardRecoverComplete(bool success) {
  DCHECK(!startup_status_.file_monitor_ok.has_value());
  startup_status_.file_monitor_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnModelReady(bool success) {
  DCHECK(!startup_status_.model_ok.has_value());
  startup_status_.model_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnModelHardRecoverComplete(bool success) {
  DCHECK(!startup_status_.model_ok.has_value());
  startup_status_.model_ok = success;
  AttemptToFinalizeSetup();
}

void ControllerImpl::OnItemAdded(bool success,
                                 DownloadClient client,
                                 const std::string& guid) {
  // If the StartCallback doesn't exist, we already notified the Client about
  // this item.  That means something went wrong, so stop here.
  if (start_callbacks_.find(guid) == start_callbacks_.end())
    return;

  if (!success) {
    HandleStartDownloadResponse(client, guid,
                                DownloadParams::StartResult::INTERNAL_ERROR);
    return;
  }

  HandleStartDownloadResponse(client, guid,
                              DownloadParams::StartResult::ACCEPTED);

  Entry* entry = model_->Get(guid);
  DCHECK(entry);
  DCHECK_EQ(Entry::State::NEW, entry->state);
  TransitTo(entry, Entry::State::AVAILABLE, model_.get());

  ActivateMoreDownloads();
}

void ControllerImpl::OnItemUpdated(bool success,
                                   DownloadClient client,
                                   const std::string& guid) {
  Entry* entry = model_->Get(guid);
  DCHECK(entry);

  // Now that we're sure that our state is set correctly, it is OK to remove the
  // DriverEntry.  If we restart we'll see a COMPLETE state and handle it
  // accordingly.
  if (entry->state == Entry::State::COMPLETE)
    driver_->Remove(guid, false);

  // TODO(dtrainor): If failed, clean up any download state accordingly.

  log_sink_->OnServiceDownloadChanged(guid);
}

void ControllerImpl::OnItemRemoved(bool success,
                                   DownloadClient client,
                                   const std::string& guid) {
  // TODO(dtrainor): If failed, clean up any download state accordingly.
}

Controller::State ControllerImpl::GetControllerState() {
  return controller_state_;
}

const StartupStatus& ControllerImpl::GetStartupStatus() {
  return startup_status_;
}

LogSource::EntryDetailsList ControllerImpl::GetServiceDownloads() {
  EntryDetailsList list;

  if (controller_state_ != State::READY)
    return list;

  auto entries = model_->PeekEntries();
  for (auto* entry : entries) {
    list.push_back(std::make_pair(entry, driver_->Find(entry->guid)));
  }
  return list;
}

std::optional<LogSource::EntryDetails> ControllerImpl::GetServiceDownload(
    const std::string& guid) {
  if (controller_state_ != State::READY)
    return std::nullopt;

  auto* entry = model_->Get(guid);
  auto driver_entry = driver_->Find(guid);

  return std::optional<LogSource::EntryDetails>(
      std::make_pair(entry, driver_entry));
}

bool ControllerImpl::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                                  base::trace_event::ProcessMemoryDump* pmd) {
  auto* dump = pmd->CreateAllocatorDump(
      base::StringPrintf("components/download/controller_0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this)));

  size_t memory_cost =
      base::trace_event::EstimateMemoryUsage(externally_active_downloads_);
  memory_cost += model_->EstimateMemoryUsage();
  memory_cost += driver_->EstimateMemoryUsage();

  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(memory_cost));
  return true;
}

void ControllerImpl::OnDeviceStatusChanged(const DeviceStatus& device_status) {
  if (controller_state_ != State::READY)
    return;

  UpdateDriverStates();
  ActivateMoreDownloads();
}

void ControllerImpl::AttemptToFinalizeSetup() {
  DCHECK(controller_state_ == State::INITIALIZING ||
         controller_state_ == State::RECOVERING);

  // Always notify the LogSink no matter what path this function takes.
  absl::Cleanup state_notifier = [this] {
    log_sink_->OnServiceStatusChanged();
  };

  if (!startup_status_.Complete())
    return;

  bool in_recovery = controller_state_ == State::RECOVERING;

  stats::LogControllerStartupStatus(in_recovery, startup_status_);
  if (!startup_status_.Ok()) {
    if (in_recovery) {
      HandleUnrecoverableSetup();
      NotifyServiceOfStartup();
    } else {
      StartHardRecoveryAttempt();
    }

    return;
  }

  device_status_listener_->SetObserver(this);
  device_status_listener_->Start(config_->network_startup_delay);
  PollActiveDriverDownloads();
  CancelOrphanedRequests();
  CleanupUnknownFiles();
  RemoveCleanupEligibleDownloads();
  ResolveInitialRequestStates();

  NotifyClientsOfStartup(in_recovery);

  controller_state_ = State::READY;

  log_sink_->OnServiceDownloadsAvailable();

  UpdateDriverStates();

  KillTimedOutDownloads();
  NotifyServiceOfStartup();

  // Pull the initial straw if active downloads haven't reach maximum.
  ActivateMoreDownloads();
}

void ControllerImpl::HandleUnrecoverableSetup() {
  controller_state_ = State::UNAVAILABLE;

  // If we cannot recover, notify Clients that the service is unavailable.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ControllerImpl::SendOnServiceUnavailable,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ControllerImpl::StartHardRecoveryAttempt() {
  startup_status_.Reset();
  controller_state_ = State::RECOVERING;

  driver_->HardRecover();
  model_->HardRecover();
  file_monitor_->HardRecover(
      base::BindOnce(&ControllerImpl::OnFileMonitorHardRecoverComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ControllerImpl::PollActiveDriverDownloads() {
  DCHECK(controller_state_ == State::INITIALIZING ||
         controller_state_ == State::RECOVERING);

  std::set<std::string> guids = driver_->GetActiveDownloads();

  for (auto guid : guids) {
    if (!model_->Get(guid))
      externally_active_downloads_.insert(guid);
  }
}

void ControllerImpl::CancelOrphanedRequests() {
  DCHECK(controller_state_ == State::INITIALIZING ||
         controller_state_ == State::RECOVERING);

  auto entries = model_->PeekEntries();

  std::vector<std::string> guids_to_remove;
  std::set<base::FilePath> files_to_remove;
  for (auto* entry : entries) {
    if (!clients_->GetClient(entry->client)) {
      guids_to_remove.push_back(entry->guid);
      files_to_remove.insert(entry->target_file_path);
    }
  }

  for (const auto& guid : guids_to_remove) {
    driver_->Remove(guid, false);
    model_->Remove(guid);
  }

  file_monitor_->DeleteFiles(files_to_remove,
                             stats::FileCleanupReason::ORPHANED);
}

void ControllerImpl::CleanupUnknownFiles() {
  DCHECK(controller_state_ == State::INITIALIZING ||
         controller_state_ == State::RECOVERING);

  auto entries = model_->PeekEntries();
  std::vector<DriverEntry> driver_entries;
  for (auto* entry : entries) {
    std::optional<DriverEntry> driver_entry = driver_->Find(entry->guid);
    if (driver_entry.has_value())
      driver_entries.push_back(driver_entry.value());
  }

  file_monitor_->DeleteUnknownFiles(entries, driver_entries, base::DoNothing());
}

void ControllerImpl::ResolveInitialRequestStates() {
  DCHECK(controller_state_ == State::INITIALIZING ||
         controller_state_ == State::RECOVERING);

  auto entries = model_->PeekEntries();
  for (auto* entry : entries) {
    // Pull the initial Entry::State and DriverEntry::State.
    Entry::State state = entry->state;
    auto driver_entry = driver_->Find(entry->guid);
    std::optional<DriverEntry::State> driver_state;
    if (driver_entry.has_value()) {
      DCHECK_NE(DriverEntry::State::UNKNOWN, driver_entry->state);
      driver_state = driver_entry->state;
    }

    // Determine what the new Entry::State should be based on the two original
    // states of the two different systems.
    Entry::State new_state = state;
    switch (state) {
      case Entry::State::NEW:
        // This means we shut down but may have not ACK'ed the download.  That
        // is OK, we will still notify the Client about the GUID when we send
        // them our initialize method.
        new_state = Entry::State::AVAILABLE;
        break;
      case Entry::State::COMPLETE:
        // We're already in our end state.  Just stay here.
        new_state = Entry::State::COMPLETE;
        break;
      case Entry::State::AVAILABLE:  // Intentional fallthrough.
      case Entry::State::ACTIVE:     // Intentional fallthrough.
      case Entry::State::PAUSED: {
        // All three of these states are effectively driven by the DriverEntry
        // state.
        if (!driver_state.has_value()) {
          // If we don't have a DriverEntry::State, just leave the state alone.
          new_state = state;
          break;
        }

        // We didn't persist the response headers in time, so just restart the
        // the fetch. The driver entry and the temporary file will also be
        // deleted.
        if (state == Entry::State::ACTIVE && entry->require_response_headers &&
            !entry->did_received_response) {
          driver_->Remove(entry->guid, true /* remove_file */);
          break;
        }

        // If we have a real DriverEntry::State, we need to determine which of
        // those states makes sense for our Entry.  Our entry can either be in
        // two states now: It's effective 'active' state (ACTIVE or PAUSED) or
        // COMPLETE.
        bool is_paused = state == Entry::State::PAUSED;
        Entry::State active =
            is_paused ? Entry::State::PAUSED : Entry::State::ACTIVE;

        switch (driver_state.value()) {
          case DriverEntry::State::IN_PROGRESS:  // Intentional fallthrough.
          case DriverEntry::State::INTERRUPTED:
            // The DriverEntry isn't done, so we need to set the Entry to the
            // 'active' state.
            new_state =
                entry->has_upload_data ? Entry::State::COMPLETE : active;
            break;
          case DriverEntry::State::COMPLETE:  // Intentional fallthrough.
          // TODO(dtrainor, xingliu) Revisit this CANCELLED state to make sure
          // all embedders behave properly.
          case DriverEntry::State::CANCELLED:
            // The DriverEntry is done.  We need to set the Entry to the
            // COMPLETE state.
            new_state = Entry::State::COMPLETE;
            break;
          default:
            NOTREACHED_IN_MIGRATION();
            break;
        }
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    // Update the Entry::State to the new correct state.
    if (new_state != entry->state) {
      TransitTo(entry, new_state, model_.get());
    }

    // Given the new correct state, update the DriverEntry to reflect the Entry.
    switch (new_state) {
      case Entry::State::NEW:        // Intentional fallthrough.
      case Entry::State::AVAILABLE:  // Intentional fallthrough.
        // We should not have a DriverEntry here.
        if (driver_entry.has_value())
          driver_->Remove(entry->guid, false);
        break;
      case Entry::State::ACTIVE:  // Intentional fallthrough.
      case Entry::State::PAUSED:
        // We're in the correct state.  Let UpdateDriverStates() restart us if
        // it wants to.
        break;
      case Entry::State::COMPLETE:
        if (state != Entry::State::COMPLETE) {
          // We are changing states to COMPLETE.  Handle this like a normal
          // completed download.

          // Treat CANCELLED and INTERRUPTED as failures.  We have to assume the
          // DriverEntry might not have persisted in time.
          CompletionType completion_type =
              (!driver_entry.has_value() ||
               driver_entry->state == DriverEntry::State::CANCELLED ||
               driver_entry->state == DriverEntry::State::INTERRUPTED)
                  ? CompletionType::UNKNOWN
                  : CompletionType::SUCCEED;
          // TODO(shaktisahu) : May be set a completion type for upload.
          HandleCompleteDownload(completion_type, entry->guid);
        } else {
          // We're staying in COMPLETE.  Make sure there is no DriverEntry here.
          if (driver_entry.has_value())
            driver_->Remove(entry->guid, false);
        }
        break;
      case Entry::State::COUNT:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
}

void ControllerImpl::UpdateDriverStates() {
  DCHECK(startup_status_.Complete());

  for (auto* entry : model_->PeekEntries())
    UpdateDriverState(entry);
}

void ControllerImpl::UpdateDriverStateWithGuid(const std::string& guid) {
  Entry* entry = model_->Get(guid);
  if (entry)
    UpdateDriverState(entry);
}

void ControllerImpl::UpdateDriverState(Entry* entry) {
  DCHECK_EQ(controller_state_, State::READY);

  if ((entry->state != Entry::State::ACTIVE &&
       entry->state != Entry::State::PAUSED) ||
      pending_uploads_.find(entry->guid) != pending_uploads_.end()) {
    return;
  }

  std::optional<DriverEntry> driver_entry = driver_->Find(entry->guid);

  // Check if the DriverEntry is in a finished state already.  If so we need to
  // clean up our Entry and finish the download.
  if (driver_entry.has_value() && driver_entry->done) {
    if (driver_entry->state == DriverEntry::State::COMPLETE) {
      HandleCompleteDownload(CompletionType::SUCCEED, entry->guid);
    } else {
      HandleCompleteDownload(
          CompletionType::FAIL /* Make a more detailed failure type? */,
          entry->guid);
    }
    return;
  }

  bool active = driver_entry.has_value() &&
                driver_entry->state == DriverEntry::State::IN_PROGRESS;

  auto blockage_status = IsDownloadBlocked(entry);

  if (blockage_status.IsBlocked()) {
    if (active) {
      stats::LogEntryEvent(stats::DownloadEvent::SUSPEND);
      stats::LogDownloadPauseReason(blockage_status,
                                    false /*on_upload_data_received*/);
    }

    if (driver_entry.has_value())
      driver_->Pause(entry->guid);
  } else {
    if (!active) {
      // If we aren't active, resuming is going to cost something.  Track
      // against throttle limits.

      if (driver_entry.has_value() && driver_entry->can_resume) {
        // This is, as far as we can tell, a free resumption.
        entry->resumption_count++;
        model_->Update(*entry);

        stats::LogEntryEvent(stats::DownloadEvent::RESUME);

        if (entry->resumption_count > config_->max_resumption_count) {
          HandleCompleteDownload(CompletionType::OUT_OF_RESUMPTIONS,
                                 entry->guid);
          return;
        }
      } else {
        // This is a costly resumption.
        entry->attempt_count++;
        model_->Update(*entry);

        stats::LogEntryRetryCount(entry->attempt_count);
        stats::LogEntryEvent(stats::DownloadEvent::RETRY);

        if (entry->attempt_count > config_->max_retry_count) {
          HandleCompleteDownload(CompletionType::OUT_OF_RETRIES, entry->guid);
          return;
        }
      }
    }

    if (driver_entry.has_value()) {
      // For uploads, we should never call resume unless it is already in
      // progress, since we have to re-supply the upload data from client.
      DCHECK(!entry->has_upload_data ||
             driver_entry->state == DriverEntry::State::IN_PROGRESS);

      driver_->Resume(entry->guid);
    } else {
      stats::LogEntryEvent(stats::DownloadEvent::START);
      PrepareToStartDownload(entry);
    }
  }

  log_sink_->OnServiceDownloadChanged(entry->guid);
}

void ControllerImpl::PrepareToStartDownload(Entry* entry) {
  pending_uploads_.insert(entry->guid);

  auto* client = clients_->GetClient(entry->client);
  DCHECK(client);

  auto callback = base::BindOnce(&ControllerImpl::OnDownloadReadyToStart,
                                 weak_ptr_factory_.GetWeakPtr(), entry->guid);

  // To ensure no re-entrancy, we post the response again after receiving from
  // the client
  client->GetUploadData(entry->guid, base::BindOnce(&RunOnDownloadReadyToStart,
                                                    std::move(callback)));

  // Reset the timeout timer in case client doesn't respond.
  cancel_uploads_callback_.Reset(base::BindOnce(
      &ControllerImpl::KillTimedOutUploads, weak_ptr_factory_.GetWeakPtr()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, cancel_uploads_callback_.callback(),
      config_->pending_upload_timeout_delay);
}

void ControllerImpl::OnDownloadReadyToStart(
    const std::string& guid,
    scoped_refptr<network::ResourceRequestBody> post_body) {
  DCHECK(pending_uploads_.find(guid) != pending_uploads_.end());
  pending_uploads_.erase(guid);

  auto* entry = model_->Get(guid);
  if (!entry) {
    return;
  }

  if (post_body) {
    entry->has_upload_data = true;
    model_->Update(*entry);
  }

  auto blockage_status = IsDownloadBlocked(entry);
  if (blockage_status.IsBlocked()) {
    stats::LogDownloadPauseReason(blockage_status,
                                  true /*on_upload_data_received*/);
    return;
  }

  auto driver_entry = driver_->Find(guid);
  if (driver_entry.has_value()) {
    DVLOG(1) << "Download already exists.";
    return;
  }

  driver_->Start(entry->request_params, entry->guid, entry->target_file_path,
                 post_body,
                 net::NetworkTrafficAnnotationTag(entry->traffic_annotation));
}

DownloadBlockageStatus ControllerImpl::IsDownloadBlocked(Entry* entry) {
  DownloadBlockageStatus status;
  status.blocked_by_criteria =
      !device_status_listener_->CurrentDeviceStatus()
           .MeetsCondition(entry->scheduling_params,
                           config_->download_battery_percentage)
           .MeetsRequirements();
  status.blocked_by_downloads =
      !externally_active_downloads_.empty() &&
      entry->scheduling_params.priority <= SchedulingParams::Priority::NORMAL;

  status.blocked_by_navigation = ShouldBlockDownloadOnNavigation(entry);
  status.entry_not_active = entry->state != Entry::State::ACTIVE;
  return status;
}

void ControllerImpl::KillTimedOutUploads() {
  for (const std::string& guid : std::move(pending_uploads_))
    HandleCompleteDownload(CompletionType::UPLOAD_TIMEOUT, guid);
}

void ControllerImpl::NotifyClientsOfStartup(bool state_lost) {
  auto categorized = util::MapEntriesToMetadataForClients(
      clients_->GetRegisteredClients(), model_->PeekEntries(), driver_.get());

  for (auto client_id : clients_->GetRegisteredClients()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ControllerImpl::SendOnServiceInitialized,
                                  weak_ptr_factory_.GetWeakPtr(), client_id,
                                  state_lost, categorized[client_id]));
  }
}

void ControllerImpl::NotifyServiceOfStartup() {
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "download_service", "DownloadServiceInitialize", TRACE_ID_LOCAL(this));

  if (init_callback_.is_null())
    return;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(init_callback_));
}

void ControllerImpl::HandleStartDownloadResponse(
    DownloadClient client,
    const std::string& guid,
    DownloadParams::StartResult result) {
  DownloadParams::StartCallback callback = std::move(start_callbacks_[guid]);
  start_callbacks_.erase(guid);
  HandleStartDownloadResponse(client, guid, result, std::move(callback));
}

void ControllerImpl::HandleStartDownloadResponse(
    DownloadClient client,
    const std::string& guid,
    DownloadParams::StartResult result,
    DownloadParams::StartCallback callback) {
  stats::LogStartDownloadResult(client, result);

  // UNEXPECTED_GUID means the guid was already in use.  Don't remove this entry
  // from the model because it's there due to another request.
  if (result != DownloadParams::StartResult::ACCEPTED &&
      result != DownloadParams::StartResult::UNEXPECTED_GUID &&
      model_->Get(guid) != nullptr) {
    model_->Remove(guid);
  }

  log_sink_->OnServiceRequestMade(client, guid, result);

  if (callback.is_null())
    return;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), guid, result));
}

void ControllerImpl::HandleCompleteDownload(CompletionType type,
                                            const std::string& guid) {
  Entry* entry = model_->Get(guid);
  DCHECK(entry);

  if (entry->state == Entry::State::COMPLETE) {
    DVLOG(1) << "Download is already completed.";
    return;
  }

  auto driver_entry = driver_->Find(guid);
  uint64_t file_size =
      driver_entry.has_value() ? driver_entry->bytes_downloaded : 0;
  stats::LogDownloadCompletion(entry->client, type, file_size);
  LOG(WARNING) << "Background download complete, client: "
               << static_cast<int>(entry->client)
               << ", completion type: " << static_cast<int>(type)
               << ", file size:" << file_size;

  if (type == CompletionType::SUCCEED) {
    DCHECK(driver_entry.has_value());
    entry->target_file_path = driver_entry->current_file_path;
    entry->completion_time = driver_entry->completion_time;
    entry->bytes_downloaded = driver_entry->bytes_downloaded;
    CompletionInfo completion_info(driver_entry->current_file_path,
                                   driver_entry->bytes_downloaded,
                                   entry->url_chain, entry->response_headers);
    completion_info.blob_handle = driver_entry->blob_handle;
    completion_info.hash256 = driver_entry->hash256;
    completion_info.custom_data = entry->custom_data;

    entry->last_cleanup_check_time = driver_entry->completion_time;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ControllerImpl::SendOnDownloadSucceeded,
                                  weak_ptr_factory_.GetWeakPtr(), entry->client,
                                  guid, completion_info));
    TransitTo(entry, Entry::State::COMPLETE, model_.get());
    ScheduleCleanupTask();
  } else {
    CompletionInfo completion_info;
    completion_info.url_chain = entry->url_chain;
    completion_info.response_headers = entry->response_headers;
    completion_info.custom_data = entry->custom_data;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ControllerImpl::SendOnDownloadFailed,
                       weak_ptr_factory_.GetWeakPtr(), entry->client, guid,
                       completion_info, FailureReasonFromCompletionType(type)));
    log_sink_->OnServiceDownloadFailed(type, *entry);

    // TODO(dtrainor): Handle the case where we crash before the model write
    // happens and we have no driver entry.
    driver_->Remove(entry->guid, false);
    model_->Remove(guid);
  }

  ActivateMoreDownloads();
}

void ControllerImpl::ScheduleCleanupTask() {
  if (task_finished_callbacks_.count(DownloadTaskType::CLEANUP_TASK) != 0)
    return;

  base::Time earliest_cleanup_start_time = base::Time::Max();
  for (const Entry* entry : model_->PeekEntries()) {
    if (entry->state != Entry::State::COMPLETE)
      continue;

    base::Time cleanup_time_for_entry =
        std::min(entry->last_cleanup_check_time + config_->file_keep_alive_time,
                 entry->completion_time + config_->max_file_keep_alive_time);
    cleanup_time_for_entry =
        std::max(cleanup_time_for_entry, base::Time::Now());
    if (cleanup_time_for_entry < earliest_cleanup_start_time) {
      earliest_cleanup_start_time = cleanup_time_for_entry;
    }
  }

  if (earliest_cleanup_start_time == base::Time::Max()) {
    task_scheduler_->CancelTask(DownloadTaskType::CLEANUP_TASK);
    return;
  }

  base::TimeDelta start_time = earliest_cleanup_start_time - base::Time::Now();
  base::TimeDelta end_time = start_time + config_->file_cleanup_window;
  DCHECK_LT(std::ceil(start_time.InSecondsF()),
            std::ceil(end_time.InSecondsF()))
      << "GCM requires start time to be less than end time";

  task_scheduler_->ScheduleTask(DownloadTaskType::CLEANUP_TASK, false, false,
                                DeviceStatus::kBatteryPercentageAlwaysStart,
                                std::ceil(start_time.InSecondsF()),
                                std::ceil(end_time.InSecondsF()));
}

void ControllerImpl::ScheduleKillDownloadTaskIfNecessary() {
  base::Time earliest_cancel_time = base::Time::Max();
  for (const Entry* entry : model_->PeekEntries()) {
    if (entry->state != Entry::State::COMPLETE &&
        entry->scheduling_params.cancel_time < earliest_cancel_time) {
      earliest_cancel_time = entry->scheduling_params.cancel_time;
    }
  }

  if (earliest_cancel_time == base::Time::Max())
    return;

  base::TimeDelta time_to_cancel =
      earliest_cancel_time > base::Time::Now()
          ? earliest_cancel_time - base::Time::Now()
          : base::TimeDelta();

  cancel_downloads_callback_.Reset(base::BindOnce(
      &ControllerImpl::KillTimedOutDownloads, weak_ptr_factory_.GetWeakPtr()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, cancel_downloads_callback_.callback(), time_to_cancel);
}

void ControllerImpl::KillTimedOutDownloads() {
  for (const Entry* entry : model_->PeekEntries()) {
    if (entry->state != Entry::State::COMPLETE &&
        entry->scheduling_params.cancel_time <= base::Time::Now()) {
      HandleCompleteDownload(CompletionType::TIMEOUT, entry->guid);
    }
  }

  ScheduleKillDownloadTaskIfNecessary();
}

void ControllerImpl::ActivateMoreDownloads() {
  if (controller_state_ != State::READY)
    return;

  if (!device_status_listener_->is_valid_state())
    return;

  TRACE_EVENT0("download_service", "ActivateMoreDownloads");

  // Check all the entries and the configuration to throttle number of
  // downloads.
  std::map<Entry::State, uint32_t> entries_states;
  for (auto* const entry : model_->PeekEntries()) {
    entries_states[entry->state]++;
  }

  uint32_t paused_count = entries_states[Entry::State::PAUSED];
  uint32_t active_count = entries_states[Entry::State::ACTIVE];

  while (CanActivateMoreDownloads(config_.get(), active_count, paused_count)) {
    Entry* next = scheduler_->Next(
        model_->PeekEntries(), device_status_listener_->CurrentDeviceStatus());
    if (!next)
      break;

    DCHECK_EQ(Entry::State::AVAILABLE, next->state);
    TransitTo(next, Entry::State::ACTIVE, model_.get());
    active_count++;
    UpdateDriverState(next);
  }

  Model::EntryList candidates = GetRunnableEntries(model_->PeekEntries());
  bool has_actionable_downloads = false;
  for (auto* entry : candidates) {
    has_actionable_downloads |=
        device_status_listener_->CurrentDeviceStatus()
            .MeetsCondition(entry->scheduling_params,
                            config_->download_battery_percentage)
            .MeetsRequirements();
    if (has_actionable_downloads)
      break;
  }
  // Only schedule a task if we are not currently running one of the same type.
  if (task_finished_callbacks_.count(DownloadTaskType::DOWNLOAD_TASK) == 0)
    scheduler_->Reschedule(candidates);

  if (!has_actionable_downloads) {
    HandleTaskFinished(DownloadTaskType::DOWNLOAD_TASK,
                       stats::ScheduledTaskStatus::COMPLETED_NORMALLY);
  }
}

void ControllerImpl::OnNavigationEvent() {
  if (controller_state_ != State::READY)
    return;

  UpdateDriverStates();
}

bool ControllerImpl::ShouldBlockDownloadOnNavigation(Entry* entry) {
  if (!navigation_monitor_->IsNavigationInProgress())
    return false;

  bool pausable_priority =
      entry->scheduling_params.priority <= SchedulingParams::Priority::NORMAL;

  std::optional<DriverEntry> driver_entry = driver_->Find(entry->guid);
  bool new_download = !driver_entry.has_value();
  bool resumable_download =
      driver_entry.has_value() && driver_entry->can_resume;

  return pausable_priority && (new_download || resumable_download);
}

void ControllerImpl::HandleExternalDownload(const std::string& guid,
                                            bool active) {
  if (active) {
    externally_active_downloads_.insert(guid);
  } else {
    externally_active_downloads_.erase(guid);
  }

  UpdateDriverStates();
}

void ControllerImpl::SendOnServiceInitialized(
    DownloadClient client_id,
    bool state_lost,
    const std::vector<DownloadMetaData>& downloads) {
  auto* client = clients_->GetClient(client_id);
  DCHECK(client);
  client->OnServiceInitialized(state_lost, downloads);
}

void ControllerImpl::SendOnServiceUnavailable() {
  for (auto client_id : clients_->GetRegisteredClients()) {
    clients_->GetClient(client_id)->OnServiceUnavailable();
  }
}

void ControllerImpl::SendOnDownloadUpdated(DownloadClient client_id,
                                           const std::string& guid,
                                           uint64_t bytes_uploaded,
                                           uint64_t bytes_downloaded) {
  if (!model_->Get(guid))
    return;

  auto* client = clients_->GetClient(client_id);
  DCHECK(client);
  client->OnDownloadUpdated(guid, bytes_uploaded, bytes_downloaded);
}

void ControllerImpl::SendOnDownloadSucceeded(
    DownloadClient client_id,
    const std::string& guid,
    const CompletionInfo& completion_info) {
  auto* client = clients_->GetClient(client_id);
  DCHECK(client);
  client->OnDownloadSucceeded(guid, completion_info);
}

void ControllerImpl::SendOnDownloadFailed(
    DownloadClient client_id,
    const std::string& guid,
    const CompletionInfo& completion_info,
    download::Client::FailureReason reason) {
  auto* client = clients_->GetClient(client_id);
  DCHECK(client);
  client->OnDownloadFailed(guid, completion_info, reason);
}

}  // namespace download

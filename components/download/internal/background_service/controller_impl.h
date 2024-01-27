// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONTROLLER_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/download/internal/background_service/controller.h"
#include "components/download/internal/background_service/download_blockage_status.h"
#include "components/download/internal/background_service/download_driver.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/log_source.h"
#include "components/download/internal/background_service/model.h"
#include "components/download/internal/background_service/scheduler/device_status_listener.h"
#include "components/download/internal/background_service/service_config_impl.h"
#include "components/download/internal/background_service/startup_status.h"
#include "components/download/internal/background_service/stats.h"
#include "components/download/public/background_service/client.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/navigation_monitor.h"
#include "components/download/public/task/task_scheduler.h"

namespace download {

class ClientSet;
class DownloadDriver;
class FileMonitor;
class LogSink;
class Model;
class NavigationMonitor;
class Scheduler;

struct Configuration;
struct DownloadMetaData;
struct SchedulingParams;

// The internal Controller implementation.  This class does all of the heavy
// lifting for the BackgroundDownloadService.
class ControllerImpl : public Controller,
                       public DownloadDriver::Client,
                       public Model::Client,
                       public DeviceStatusListener::Observer,
                       public NavigationMonitor::Observer,
                       public LogSource,
                       public base::trace_event::MemoryDumpProvider {
 public:
  // |config| and |log_sink| are externally owned and must be guaranteed to
  // outlive this class.
  ControllerImpl(std::unique_ptr<Configuration> config,
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
                 const base::FilePath& download_file_dir);

  ControllerImpl(const ControllerImpl&) = delete;
  ControllerImpl& operator=(const ControllerImpl&) = delete;

  ~ControllerImpl() override;

  // Controller implementation.
  void Initialize(base::OnceClosure callback) override;
  const ServiceConfig& GetConfig() override;
  BackgroundDownloadService::ServiceStatus GetStatus() override;
  State GetState() override;
  void StartDownload(DownloadParams params) override;
  void PauseDownload(const std::string& guid) override;
  void ResumeDownload(const std::string& guid) override;
  void CancelDownload(const std::string& guid) override;
  void ChangeDownloadCriteria(const std::string& guid,
                              const SchedulingParams& params) override;
  DownloadClient GetOwnerOfDownload(const std::string& guid) override;
  void OnStartScheduledTask(DownloadTaskType task_type,
                            TaskFinishedCallback callback) override;
  bool OnStopScheduledTask(DownloadTaskType task_type) override;
  Logger* GetLogger() override;

 private:
  // DownloadDriver::Client implementation.
  void OnDriverReady(bool success) override;
  void OnDriverHardRecoverComplete(bool success) override;
  void OnDownloadCreated(const DriverEntry& download) override;
  void OnDownloadFailed(const DriverEntry& download,
                        FailureType failure_type) override;
  void OnDownloadSucceeded(const DriverEntry& download) override;
  void OnDownloadUpdated(const DriverEntry& download) override;
  bool IsTrackingDownload(const std::string& guid) const override;
  void OnUploadProgress(const std::string& guid,
                        uint64_t bytes_uploaded) const override;

  // Model::Client implementation.
  void OnModelReady(bool success) override;
  void OnModelHardRecoverComplete(bool success) override;
  void OnItemAdded(bool success,
                   DownloadClient client,
                   const std::string& guid) override;
  void OnItemUpdated(bool success,
                     DownloadClient client,
                     const std::string& guid) override;
  void OnItemRemoved(bool success,
                     DownloadClient client,
                     const std::string& guid) override;

  // LogSource implementation.
  Controller::State GetControllerState() override;
  const StartupStatus& GetStartupStatus() override;
  LogSource::EntryDetailsList GetServiceDownloads() override;
  std::optional<EntryDetails> GetServiceDownload(
      const std::string& guid) override;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Called when the file monitor and download file directory are initialized.
  void OnFileMonitorReady(bool success);

  // Called when the file monitor finishes attempting to recover itself.
  void OnFileMonitorHardRecoverComplete(bool success);

  // DeviceStatusListener::Observer implementation.
  void OnDeviceStatusChanged(const DeviceStatus& device_status) override;

  // NavigationMonitor::Observer implementation.
  void OnNavigationEvent() override;

  // Checks if initialization is complete and successful.  If so, completes the
  // internal state initialization.
  void AttemptToFinalizeSetup();

  // Called when setup and recovery failed.  Shuts down the service and notifies
  // the Clients.
  void HandleUnrecoverableSetup();

  // If initialization failed, try to reset the state of all components and
  // restart them.  If that attempt fails the service will be unavailable.
  void StartHardRecoveryAttempt();

  // Checks for all the currently active driver downloads.  This lets us know
  // which ones are active that we haven't tracked.
  void PollActiveDriverDownloads();

  // Cancels and cleans upany requests that are no longer associated with a
  // Client in |clients_|.
  void CancelOrphanedRequests();

  // Cleans up any files that are left on the disk without any entries.
  void CleanupUnknownFiles();

  // Fixes any discrepancies in state between |model_| and |driver_|.  Meant to
  // resolve state issues during startup.
  void ResolveInitialRequestStates();

  // Updates the driver states based on the states of entries in download
  // service.
  void UpdateDriverStates();

  // See |UpdateDriverState|.
  void UpdateDriverStateWithGuid(const std::string& guid);

  // Processes the download based on the state of |entry|. May start, pause
  // or resume a download accordingly.
  void UpdateDriverState(Entry* entry);

  // Returns a struct representing various reasons whether the download cannot
  // start or continue at this time.
  DownloadBlockageStatus IsDownloadBlocked(Entry* entry);

  // Notifies all Client in |clients_| that this controller is initialized and
  // lets them know which download requests we are aware of for their
  // DownloadClient.
  void NotifyClientsOfStartup(bool state_lost);

  // Notifies the service that the startup has completed so that it can start
  // processing any pending requests.
  void NotifyServiceOfStartup();

  void HandleStartDownloadResponse(DownloadClient client,
                                   const std::string& guid,
                                   DownloadParams::StartResult result);
  void HandleStartDownloadResponse(DownloadClient client,
                                   const std::string& guid,
                                   DownloadParams::StartResult result,
                                   DownloadParams::StartCallback callback);

  // Handles and clears any pending task finished callbacks.
  void HandleTaskFinished(DownloadTaskType task_type,
                          stats::ScheduledTaskStatus status);
  void OnCompleteCleanupTask();

  void HandleCompleteDownload(CompletionType type, const std::string& guid);

  // Find more available entries to download, until the number of active entries
  // reached maximum.
  void ActivateMoreDownloads();

  // Whether the download should be paused or postponed in case of an active
  // navigation is in progress.
  bool ShouldBlockDownloadOnNavigation(Entry* entry);

  void RemoveCleanupEligibleDownloads();

  void HandleExternalDownload(const std::string& guid, bool active);
  void PrepareToStartDownload(Entry* entry);
  void OnDownloadReadyToStart(
      const std::string& guid,
      scoped_refptr<network::ResourceRequestBody> post_body);

  // Postable methods meant to just be pass throughs to Client APIs.  This is
  // meant to help prevent reentrancy.
  void SendOnServiceInitialized(DownloadClient client_id,
                                bool state_lost,
                                const std::vector<DownloadMetaData>& downloads);
  void SendOnServiceUnavailable();
  void SendOnDownloadUpdated(DownloadClient client_id,
                             const std::string& guid,
                             uint64_t bytes_uploaded,
                             uint64_t bytes_downloaded);
  void SendOnDownloadSucceeded(DownloadClient client_id,
                               const std::string& guid,
                               const CompletionInfo& completion_info);
  void SendOnDownloadFailed(DownloadClient client_id,
                            const std::string& guid,
                            const CompletionInfo& completion_info,
                            download::Client::FailureReason reason);

  // Schedules a cleanup task in future based on status of entries.
  void ScheduleCleanupTask();

  // Posts a task to cancel the downloads in future based on the cancel_after
  // time of the entries. If cancel time for an entry is already surpassed, the
  // task will be posted right away which will clean the entry.
  void ScheduleKillDownloadTaskIfNecessary();

  // Kills the downloads which have surpassed their cancel_after time.
  void KillTimedOutDownloads();

  // A periodical task that will kill all the pending uploads that haven't
  // received upload data from their respective clients.
  void KillTimedOutUploads();

  // The directory in which the downloaded files are stored.
  const base::FilePath download_file_dir_;

  // Owned Dependencies.
  std::unique_ptr<Configuration> config_;
  ServiceConfigImpl service_config_;
  std::unique_ptr<Logger> logger_;
  raw_ptr<LogSink, DanglingUntriaged> log_sink_;
  std::unique_ptr<ClientSet> clients_;
  std::unique_ptr<DownloadDriver> driver_;
  std::unique_ptr<Model> model_;
  std::unique_ptr<DeviceStatusListener> device_status_listener_;
  raw_ptr<NavigationMonitor> navigation_monitor_;
  std::unique_ptr<Scheduler> scheduler_;
  std::unique_ptr<TaskScheduler> task_scheduler_;
  std::unique_ptr<FileMonitor> file_monitor_;

  // Internal state.
  base::OnceClosure init_callback_;
  State controller_state_;
  StartupStatus startup_status_;
  std::set<std::string> externally_active_downloads_;
  std::set<std::string> pending_uploads_;
  std::map<std::string, DownloadParams::StartCallback> start_callbacks_;
  std::map<DownloadTaskType, TaskFinishedCallback> task_finished_callbacks_;
  base::CancelableOnceClosure cancel_downloads_callback_;
  base::CancelableOnceClosure cancel_uploads_callback_;

  // Only used to post tasks on the same thread.
  base::WeakPtrFactory<ControllerImpl> weak_ptr_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONTROLLER_IMPL_H_

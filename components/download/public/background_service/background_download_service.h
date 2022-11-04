// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_BACKGROUND_DOWNLOAD_SERVICE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_BACKGROUND_DOWNLOAD_SERVICE_H_

#include <memory>
#include <string>

#include "base/task/sequenced_task_runner.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/task/download_task_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace download {

class Client;
class Logger;
class ServiceConfig;

struct DownloadParams;
struct SchedulingParams;

using TaskFinishedCallback = base::OnceCallback<void(bool)>;

#if BUILDFLAG(IS_IOS)
// Identifier for background download service.
extern const char kBackgroundDownloadIdentifierPrefix[];
#endif  // BUILDFLAG(IS_IOS)

// A service responsible for helping facilitate the scheduling and downloading
// of file content from the web.  See |DownloadParams| for more details on the
// types of scheduling that can be achieved and the required input parameters
// for starting a download.  Note that BackgroundDownloadService with a valid
// storage directory will persist the requests across restarts.  This means that
// any feature requesting a download will have to implement a download::Client
// interface so this class knows who to contact when a download completes after
// a process restart.
// See the embedder specific factories for creation options.
class BackgroundDownloadService : public KeyedService {
 public:
  // The current status of the Service.
  enum class ServiceStatus {
    // The service is in the process of initializing and should not be used yet.
    // All registered Clients will be notified via
    // Client::OnServiceInitialized() once the service is ready.
    STARTING_UP = 0,

    // The service is ready and available for use.
    READY = 1,

    // The service is unavailable.  This is typically due to an unrecoverable
    // error on some internal component like the persistence layer.
    UNAVAILABLE = 2,
  };

  // Returns useful configuration information about the DownloadService. Not
  // supported on iOS.
  virtual const ServiceConfig& GetConfig() = 0;

  // Callback method to run by the service when a pre-scheduled task starts.
  // This method is invoked on main thread and while it is running, the system
  // holds a wakelock which is not released until either the |callback| is run
  // or OnStopScheduledTask is invoked by the system. Do not call this method
  // directly. Not supported on iOS.
  virtual void OnStartScheduledTask(DownloadTaskType task_type,
                                    TaskFinishedCallback callback) = 0;

  // Callback method to run by the service if the system decides to stop the
  // task. Returns true if the task needs to be rescheduled. Any pending
  // TaskFinishedCallback should be reset after this call. Do not call this
  // method directly. Not supported on iOS.
  virtual bool OnStopScheduledTask(DownloadTaskType task_type) = 0;

  // Whether or not the BackgroundDownloadService is currently available,
  // initialized successfully, and ready to be used.
  virtual ServiceStatus GetStatus() = 0;

  // Sends the download to the service.  A callback to
  // |DownloadParams::callback| will be triggered once the download has been
  // persisted and saved in the service.
  virtual void StartDownload(DownloadParams download_params) = 0;

  // Allows any feature to pause or resume downloads at will.  Paused downloads
  // will not start or stop based on scheduling criteria.  They will be
  // effectively frozen. Not supported on iOS.
  virtual void PauseDownload(const std::string& guid) = 0;
  virtual void ResumeDownload(const std::string& guid) = 0;

  // Cancels a download in this service.  The canceled download will be
  // interrupted if it is running. Not supported on iOS.
  virtual void CancelDownload(const std::string& guid) = 0;

  // Changes the current scheduling criteria for a download.  This is useful if
  // a user action might constrain or loosen the device state during which this
  // download can run. Not supported on iOS.
  virtual void ChangeDownloadCriteria(const std::string& guid,
                                      const SchedulingParams& params) = 0;

  // Returns a Logger instance that is meant to be used by logging and debug UI
  // components in the larger system.
  virtual Logger* GetLogger() = 0;

#if BUILDFLAG(IS_IOS)
  // Called by the  system to handle events for background URL session. Once
  // done, the passed function should be called.
  virtual void HandleEventsForBackgroundURLSession(base::OnceClosure) {}
#endif  // BUILDFLAG(IS_IOS)

  BackgroundDownloadService(const BackgroundDownloadService&) = delete;
  BackgroundDownloadService& operator=(const BackgroundDownloadService&) =
      delete;

  ~BackgroundDownloadService() override = default;

 protected:
  BackgroundDownloadService() = default;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_BACKGROUND_DOWNLOAD_SERVICE_H_

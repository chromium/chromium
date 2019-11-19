// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/printer_job_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/cloud_print/cloud_print_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/service/cloud_print/cloud_print_service_helpers.h"
#include "chrome/service/cloud_print/job_status_updater.h"
#include "net/base/mime_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "printing/printing_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace cloud_print {

namespace {

base::subtle::Atomic32 g_total_jobs_started = 0;
base::subtle::Atomic32 g_total_jobs_done = 0;

enum PrinterJobHandlerEvent {
  JOB_HANDLER_CHECK_FOR_JOBS,
  JOB_HANDLER_START,
  JOB_HANDLER_PENDING_TASK,
  JOB_HANDLER_PRINTER_UPDATE,
  JOB_HANDLER_JOB_CHECK,
  JOB_HANDLER_JOB_STARTED,
  JOB_HANDLER_VALID_TICKET,
  JOB_HANDLER_DATA,
  JOB_HANDLER_SET_IN_PROGRESS,
  JOB_HANDLER_SET_START_PRINTING,
  JOB_HANDLER_START_SPOOLING,
  JOB_HANDLER_SPOOLED,
  JOB_HANDLER_JOB_COMPLETED,
  JOB_HANDLER_INVALID_TICKET,
  JOB_HANDLER_INVALID_DATA,
  JOB_HANDLER_MAX,
};

constexpr net::PartialNetworkTrafficAnnotationTag kPartialTrafficAnnotation =
    net::DefinePartialNetworkTrafficAnnotation("printer_job_handler",
                                               "cloud_print",
                                               R"(
        semantics {
          description:
            "Handles Cloud Print jobs for a particular printer, including "
            "connecting to printer, sending jobs, updating jobs, and getting "
            "status."
          trigger:
            "Automatic checking if printer is available, registering printer, "
            "and starting or continuing a printer task."
          data:
            "Cloud Print server URL, printer id, job details."
        })");

}  // namespace

PrinterJobHandler::PrinterInfoFromCloud::PrinterInfoFromCloud() = default;

PrinterJobHandler::PrinterInfoFromCloud::PrinterInfoFromCloud(
    const PrinterInfoFromCloud& other) = default;

PrinterJobHandler::PrinterJobHandler(
    const printing::PrinterBasicInfo& printer_info,
    const PrinterInfoFromCloud& printer_info_cloud,
    const GURL& cloud_print_server_url,
    PrintSystem* print_system,
    Delegate* delegate)
    : print_system_(print_system),
      printer_info_(printer_info),
      printer_info_cloud_(printer_info_cloud),
      cloud_print_server_url_(cloud_print_server_url),
      delegate_(delegate),
      print_thread_("Chrome_CloudPrintJobPrintThread"),
      job_handler_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(delegate_);
}

bool PrinterJobHandler::Initialize() {
  if (!print_system_->IsValidPrinter(printer_info_.printer_name))
    return false;

  printer_watcher_ = print_system_->CreatePrinterWatcher(
      printer_info_.printer_name);
  printer_watcher_->StartWatching(this);
  CheckForJobs(kJobFetchReasonStartup);
  return true;
}

const std::string& PrinterJobHandler::GetPrinterName() const {
  return printer_info_.printer_name;
}

void PrinterJobHandler::CheckForJobs(const std::string& reason) {
  VLOG(1) << "CP_CONNECTOR: Checking for jobs"
          << ", printer id: " << printer_info_cloud_.printer_id
          << ", reason: " << reason
          << ", task in progress: " << task_in_progress_;
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                            JOB_HANDLER_CHECK_FOR_JOBS, JOB_HANDLER_MAX);
  job_fetch_reason_ = reason;
  job_check_pending_ = true;
  if (!task_in_progress_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PrinterJobHandler::Start, this));
  }
}

void PrinterJobHandler::Shutdown() {
  VLOG(1) << "CP_CONNECTOR: Shutting down printer job handler"
          << ", printer id: " << printer_info_cloud_.printer_id;
  Reset();
  shutting_down_ = true;
  while (!job_status_updater_list_.empty()) {
    // Calling Stop() will cause the OnJobCompleted to be called which will
    // remove the updater object from the list.
    job_status_updater_list_.front()->Stop();
  }
}

// CloudPrintURLFetcher::Delegate implementation.
CloudPrintURLFetcher::ResponseAction PrinterJobHandler::HandleRawResponse(
    const net::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const std::string& data) {
  // 415 (Unsupported media type) error while fetching data from the server
  // means data conversion error. Stop fetching process and mark job as error.
  if (next_data_handler_ == (&PrinterJobHandler::HandlePrintDataResponse) &&
      response_code == net::HTTP_UNSUPPORTED_MEDIA_TYPE) {
    VLOG(1) << "CP_CONNECTOR: Job failed (unsupported media type)";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PrinterJobHandler::JobFailed, this,
                                  JOB_DOWNLOAD_FAILED));
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction PrinterJobHandler::HandleRawData(
    const net::URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  if (!next_data_handler_)
    return CloudPrintURLFetcher::CONTINUE_PROCESSING;
  return (this->*next_data_handler_)(source, url, data);
}

CloudPrintURLFetcher::ResponseAction PrinterJobHandler::HandleJSONData(
    const net::URLFetcher* source,
    const GURL& url,
    const base::Value& json_data,
    bool succeeded) {
  DCHECK(next_json_data_handler_);
  return (this->*next_json_data_handler_)(source, url, json_data, succeeded);
}

// Mark the job fetch as failed and check if other jobs can be printed
void PrinterJobHandler::OnRequestGiveUp() {
  if (job_queue_handler_.JobFetchFailed(job_details_.job_id_)) {
    VLOG(1) << "CP_CONNECTOR: Job failed to load (scheduling retry)";
    CheckForJobs(kJobFetchReasonFailure);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PrinterJobHandler::Stop, this));
  } else {
    VLOG(1) << "CP_CONNECTOR: Job failed (giving up after " <<
        kNumRetriesBeforeAbandonJob << " retries)";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PrinterJobHandler::JobFailed, this,
                                  JOB_DOWNLOAD_FAILED));
  }
}

CloudPrintURLFetcher::ResponseAction PrinterJobHandler::OnRequestAuthError() {
  // We got an Auth error and have no idea how long it will take to refresh
  // auth information (may take forever). We'll drop current request and
  // propagate this error to the upper level. After auth issues will be
  // resolved, GCP connector will restart.
  OnAuthError();
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

std::string PrinterJobHandler::GetAuthHeader() {
  return GetCloudPrintAuthHeaderFromStore();
}

// JobStatusUpdater::Delegate implementation
bool PrinterJobHandler::OnJobCompleted(JobStatusUpdater* updater) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                            JOB_HANDLER_JOB_COMPLETED, JOB_HANDLER_MAX);
  UMA_HISTOGRAM_LONG_TIMES("CloudPrint.PrintingTime",
                           base::Time::Now() - updater->start_time());
  base::subtle::NoBarrier_AtomicIncrement(&g_total_jobs_done, 1);
  job_queue_handler_.JobDone(job_details_.job_id_);

  for (auto it = job_status_updater_list_.begin();
       it != job_status_updater_list_.end(); ++it) {
    if (it->get() == updater) {
      job_status_updater_list_.erase(it);
      return true;
    }
  }
  return false;
}

void PrinterJobHandler::OnAuthError() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PrinterJobHandler::Stop, this));
  delegate_->OnAuthError();
}

void PrinterJobHandler::OnPrinterDeleted() {
  delegate_->OnPrinterDeleted(printer_info_cloud_.printer_id);
}

void PrinterJobHandler::OnPrinterChanged() {
  printer_update_pending_ = true;
  if (!task_in_progress_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PrinterJobHandler::Start, this));
  }
}

void PrinterJobHandler::OnJobChanged() {
  // Some job on the printer changed. Loop through all our JobStatusUpdaters
  // and have them check for updates.
  for (const auto& it : job_status_updater_list_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&JobStatusUpdater::UpdateStatus, it));
  }
}

void PrinterJobHandler::OnJobSpoolSucceeded(const PlatformJobId& job_id) {
  DCHECK(CurrentlyOnPrintThread());
  print_thread_.task_runner()->ReleaseSoon(FROM_HERE, std::move(job_spooler_));
  job_handler_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PrinterJobHandler::JobSpooled, this, job_id));
}

void PrinterJobHandler::OnJobSpoolFailed() {
  DCHECK(CurrentlyOnPrintThread());
  print_thread_.task_runner()->ReleaseSoon(FROM_HERE, std::move(job_spooler_));
  VLOG(1) << "CP_CONNECTOR: Job failed (spool failed)";
  job_handler_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterJobHandler::JobFailed, this, JOB_FAILED));
}

// static
void PrinterJobHandler::ReportsStats() {
  base::subtle::Atomic32 started =
      base::subtle::NoBarrier_AtomicExchange(&g_total_jobs_started, 0);
  base::subtle::Atomic32 done =
      base::subtle::NoBarrier_AtomicExchange(&g_total_jobs_done, 0);
  UMA_HISTOGRAM_COUNTS_100("CloudPrint.JobsStartedPerInterval", started);
  UMA_HISTOGRAM_COUNTS_100("CloudPrint.JobsDonePerInterval", done);
}

PrinterJobHandler::~PrinterJobHandler() {
  if (printer_watcher_.get())
    printer_watcher_->StopWatching();
}

// Begin Response handlers
CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandlePrinterUpdateResponse(const net::URLFetcher* source,
                                               const GURL& url,
                                               const base::Value& json_data,
                                               bool succeeded) {
  VLOG(1) << "CP_CONNECTOR: Handling printer update response"
          << ", printer id: " << printer_info_cloud_.printer_id;
  // We are done here. Go to the Stop state
  VLOG(1) << "CP_CONNECTOR: Stopping printer job handler"
          << ", printer id: " << printer_info_cloud_.printer_id;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PrinterJobHandler::Stop, this));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandleJobMetadataResponse(const net::URLFetcher* source,
                                             const GURL& url,
                                             const base::Value& json_data,
                                             bool succeeded) {
  VLOG(1) << "CP_CONNECTOR: Handling job metadata response"
          << ", printer id: " << printer_info_cloud_.printer_id;
  if (succeeded) {
    std::vector<JobDetails> jobs =
        job_queue_handler_.GetJobsFromQueue(json_data);
    if (!jobs.empty()) {
      if (jobs[0].time_remaining_.is_zero()) {
        job_details_ = jobs[0];
        job_start_time_ = base::Time::Now();
        base::subtle::NoBarrier_AtomicIncrement(&g_total_jobs_started, 1);
        UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                                  JOB_HANDLER_JOB_STARTED, JOB_HANDLER_MAX);
        SetNextDataHandler(&PrinterJobHandler::HandlePrintTicketResponse);
        request_ = CloudPrintURLFetcher::Create(kPartialTrafficAnnotation);
        GURL request_url;
        if (print_system_->UseCddAndCjt()) {
          request_url = GetUrlForJobCjt(
              cloud_print_server_url_, job_details_.job_id_, job_fetch_reason_);
        } else {
          request_url = GURL(job_details_.print_ticket_url_);
        }
        request_->StartGetRequest(CloudPrintURLFetcher::REQUEST_TICKET,
                                  request_url, this, kJobDataMaxRetryCount,
                                  std::string());
        return CloudPrintURLFetcher::STOP_PROCESSING;
      }
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PrinterJobHandler::RunScheduledJobCheck, this),
          jobs[0].time_remaining_);
    }
  }

  // If no jobs are available, go to the Stop state.
  VLOG(1) << "CP_CONNECTOR: Stopping printer job handler"
          << ", printer id: " << printer_info_cloud_.printer_id;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PrinterJobHandler::Stop, this));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandlePrintTicketResponse(const net::URLFetcher* source,
                                             const GURL& url,
                                             const std::string& data) {
  VLOG(1) << "CP_CONNECTOR: Handling print ticket response"
          << ", printer id: " << printer_info_cloud_.printer_id;
  std::string mime_type;
  source->GetResponseHeaders()->GetMimeType(&mime_type);
  if (print_system_->ValidatePrintTicket(printer_info_.printer_name, data,
                                         mime_type)) {
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                              JOB_HANDLER_VALID_TICKET, JOB_HANDLER_MAX);
    job_details_.print_ticket_ = data;
    job_details_.print_ticket_mime_type_ = mime_type;
    SetNextDataHandler(&PrinterJobHandler::HandlePrintDataResponse);
    request_ = CloudPrintURLFetcher::Create(kPartialTrafficAnnotation);
    std::string accept_headers = "Accept: ";
    accept_headers += print_system_->GetSupportedMimeTypes();
    request_->StartGetRequest(CloudPrintURLFetcher::REQUEST_DATA,
        GURL(job_details_.print_data_url_), this, kJobDataMaxRetryCount,
        accept_headers);
  } else {
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                              JOB_HANDLER_INVALID_TICKET, JOB_HANDLER_MAX);
    // The print ticket was not valid. We are done here.
    ValidatePrintTicketFailed();
  }
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandlePrintDataResponse(const net::URLFetcher* source,
                                           const GURL& url,
                                           const std::string& data) {
  VLOG(1) << "CP_CONNECTOR: Handling print data response"
          << ", printer id: " << printer_info_cloud_.printer_id;
  if (base::CreateTemporaryFile(&job_details_.print_data_file_path_)) {
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent", JOB_HANDLER_DATA,
                              JOB_HANDLER_MAX);
    int ret = base::WriteFile(job_details_.print_data_file_path_,
                              data.c_str(), data.length());
    source->GetResponseHeaders()->GetMimeType(
        &job_details_.print_data_mime_type_);
    if (ret == static_cast<int>(data.length())) {
      UpdateJobStatus(PRINT_JOB_STATUS_IN_PROGRESS, JOB_SUCCESS);
      return CloudPrintURLFetcher::STOP_PROCESSING;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                            JOB_HANDLER_INVALID_DATA, JOB_HANDLER_MAX);

  // If we are here, then there was an error in saving the print data, bail out
  // here.
  VLOG(1) << "CP_CONNECTOR: Error saving print data"
          << ", printer id: " << printer_info_cloud_.printer_id;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterJobHandler::JobFailed, this, JOB_DOWNLOAD_FAILED));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandleInProgressStatusUpdateResponse(
    const net::URLFetcher* source,
    const GURL& url,
    const base::Value& json_data,
    bool succeeded) {
  VLOG(1) << "CP_CONNECTOR: Handling success status update response"
          << ", printer id: " << printer_info_cloud_.printer_id;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PrinterJobHandler::StartPrinting, this));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandleFailureStatusUpdateResponse(
    const net::URLFetcher* source,
    const GURL& url,
    const base::Value& json_data,
    bool succeeded) {
  VLOG(1) << "CP_CONNECTOR: Handling failure status update response"
          << ", printer id: " << printer_info_cloud_.printer_id;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PrinterJobHandler::Stop, this));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

void PrinterJobHandler::Start() {
  VLOG(1) << "CP_CONNECTOR: Starting printer job handler"
          << ", printer id: " << printer_info_cloud_.printer_id
          << ", task in progress: " << task_in_progress_;
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                            JOB_HANDLER_START, JOB_HANDLER_MAX);
  if (task_in_progress_) {
    // Multiple Starts can get posted because of multiple notifications
    // We want to ignore the other ones that happen when a task is in progress.
    return;
  }
  Reset();
  if (shutting_down_)
    return;

  // Check if we have work to do.
  if (!HavePendingTasks())
    return;

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                            JOB_HANDLER_PENDING_TASK, JOB_HANDLER_MAX);
  if (!task_in_progress_ && printer_update_pending_) {
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                              JOB_HANDLER_PRINTER_UPDATE, JOB_HANDLER_MAX);
    printer_update_pending_ = false;
    task_in_progress_ = UpdatePrinterInfo();
    VLOG(1) << "CP_CONNECTOR: Changed task in progress"
            << ", printer id: " << printer_info_cloud_.printer_id
            << ", task in progress: " << task_in_progress_;
  }
  if (!task_in_progress_ && job_check_pending_) {
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                              JOB_HANDLER_JOB_CHECK, JOB_HANDLER_MAX);
    task_in_progress_ = true;
    VLOG(1) << "CP_CONNECTOR: Changed task in progress"
               ", printer id: "
            << printer_info_cloud_.printer_id
            << ", task in progress: " << task_in_progress_;
    job_check_pending_ = false;
    // We need to fetch any pending jobs for this printer
    SetNextJSONHandler(&PrinterJobHandler::HandleJobMetadataResponse);
    request_ = CloudPrintURLFetcher::Create(kPartialTrafficAnnotation);
    request_->StartGetRequest(
        CloudPrintURLFetcher::REQUEST_JOB_FETCH,
        GetUrlForJobFetch(cloud_print_server_url_,
                          printer_info_cloud_.printer_id, job_fetch_reason_),
        this, kCloudPrintAPIMaxRetryCount, std::string());
    last_job_fetch_time_ = base::TimeTicks::Now();
    VLOG(1) << "CP_CONNECTOR: Last job fetch time"
            << ", printer name: " << printer_info_.printer_name
            << ", timestamp: " << last_job_fetch_time_.since_origin();
    job_fetch_reason_.clear();
  }
}

void PrinterJobHandler::Stop() {
  VLOG(1) << "CP_CONNECTOR: Stopping printer job handler"
          << ", printer id: " << printer_info_cloud_.printer_id;
  task_in_progress_ = false;
  VLOG(1) << "CP_CONNECTOR: Changed task in progress"
          << ", printer id: " << printer_info_cloud_.printer_id
          << ", task in progress: " << task_in_progress_;
  Reset();
  if (HavePendingTasks()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PrinterJobHandler::Start, this));
  }
}

void PrinterJobHandler::StartPrinting() {
  VLOG(1) << "CP_CONNECTOR: Starting printing"
          << ", printer id: " << printer_info_cloud_.printer_id;
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                            JOB_HANDLER_SET_START_PRINTING, JOB_HANDLER_MAX);
  // We are done with the request object for now.
  request_.reset();
  if (shutting_down_)
    return;

#if defined(OS_WIN)
  print_thread_.init_com_with_mta(true);
#endif
  if (print_thread_.Start()) {
    print_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&PrinterJobHandler::DoPrint, this,
                                  job_details_, printer_info_.printer_name));
  } else {
    VLOG(1) << "CP_CONNECTOR: Failed to start print thread"
            << ", printer id: " << printer_info_cloud_.printer_id;
    JobFailed(JOB_FAILED);
  }
}

void PrinterJobHandler::Reset() {
  job_details_.Clear();
  request_.reset();
  print_thread_.Stop();
}

void PrinterJobHandler::UpdateJobStatus(PrintJobStatus status,
                                        PrintJobError error) {
  VLOG(1) << "CP_CONNECTOR: Updating job status"
          << ", printer id: " << printer_info_cloud_.printer_id
          << ", job id: " << job_details_.job_id_
          << ", job status: " << status;
  if (shutting_down_) {
    VLOG(1) << "CP_CONNECTOR: Job status update aborted (shutting down)"
            << ", printer id: " << printer_info_cloud_.printer_id
            << ", job id: " << job_details_.job_id_;
    return;
  }
  if (job_details_.job_id_.empty()) {
    VLOG(1) << "CP_CONNECTOR: Job status update aborted (empty job id)"
            << ", printer id: " << printer_info_cloud_.printer_id;
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobStatus", error, JOB_MAX);

  if (error == JOB_SUCCESS) {
    DCHECK_EQ(status, PRINT_JOB_STATUS_IN_PROGRESS);
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                              JOB_HANDLER_SET_IN_PROGRESS, JOB_HANDLER_MAX);
    SetNextJSONHandler(
        &PrinterJobHandler::HandleInProgressStatusUpdateResponse);
  } else {
    SetNextJSONHandler(
        &PrinterJobHandler::HandleFailureStatusUpdateResponse);
  }
  request_ = CloudPrintURLFetcher::Create(kPartialTrafficAnnotation);
  request_->StartGetRequest(
      CloudPrintURLFetcher::REQUEST_UPDATE_JOB,
      GetUrlForJobStatusUpdate(cloud_print_server_url_, job_details_.job_id_,
                               status, error),
      this, kCloudPrintAPIMaxRetryCount, std::string());
}

void PrinterJobHandler::RunScheduledJobCheck() {
  CheckForJobs(kJobFetchReasonRetry);
}

void PrinterJobHandler::SetNextJSONHandler(JSONDataHandler handler) {
  next_json_data_handler_ = handler;
  next_data_handler_ = NULL;
}

void PrinterJobHandler::SetNextDataHandler(DataHandler handler) {
  next_data_handler_ = handler;
  next_json_data_handler_ = NULL;
}

void PrinterJobHandler::JobFailed(PrintJobError error) {
  VLOG(1) << "CP_CONNECTOR: Job failed"
          << ", printer id: " << printer_info_cloud_.printer_id
          << ", job id: " << job_details_.job_id_
          << ", error: " << error;
  if (!shutting_down_) {
    UpdateJobStatus(PRINT_JOB_STATUS_ERROR, error);
    // This job failed, but others may be pending.  Schedule a check.
    job_check_pending_ = true;
    job_fetch_reason_ = kJobFetchReasonFailure;
  }
}

void PrinterJobHandler::JobSpooled(PlatformJobId local_job_id) {
  VLOG(1) << "CP_CONNECTOR: Job spooled"
          << ", printer id: " << printer_info_cloud_.printer_id
          << ", job id: " << local_job_id;
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent", JOB_HANDLER_SPOOLED,
                            JOB_HANDLER_MAX);
  UMA_HISTOGRAM_LONG_TIMES("CloudPrint.SpoolingTime",
                           base::Time::Now() - spooling_start_time_);
  if (shutting_down_)
    return;

  local_job_id_ = local_job_id;
  print_thread_.Stop();

  // The print job has been spooled locally. We now need to create an object
  // that monitors the status of the job and updates the server.
  auto job_status_updater = base::MakeRefCounted<JobStatusUpdater>(
      printer_info_.printer_name, job_details_.job_id_, local_job_id_,
      cloud_print_server_url_, print_system_.get(), this,
      kPartialTrafficAnnotation);
  job_status_updater_list_.push_back(job_status_updater);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&JobStatusUpdater::UpdateStatus, job_status_updater));

  CheckForJobs(kJobFetchReasonQueryMore);

  VLOG(1) << "CP_CONNECTOR: Stopping printer job handler"
          << ", printer id: " << printer_info_cloud_.printer_id;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PrinterJobHandler::Stop, this));
}

bool PrinterJobHandler::UpdatePrinterInfo() {
  if (!printer_watcher_.get()) {
    LOG(ERROR) << "CP_CONNECTOR: Printer watcher is missing."
               << " Check printer server url for printer id: "
               << printer_info_cloud_.printer_id;
    return false;
  }

  VLOG(1) << "CP_CONNECTOR: Updating printer info"
          << ", printer id: " << printer_info_cloud_.printer_id;
  // We need to update the parts of the printer info that have changed
  // (could be printer name, description, status or capabilities).
  // First asynchronously fetch the capabilities.
  printing::PrinterBasicInfo printer_info;
  printer_watcher_->GetCurrentPrinterInfo(&printer_info);

  // Asynchronously fetch the printer caps and defaults. The story will
  // continue in OnReceivePrinterCaps.
  print_system_->GetPrinterCapsAndDefaults(
      printer_info.printer_name,
      base::BindOnce(&PrinterJobHandler::OnReceivePrinterCaps,
                     weak_ptr_factory_.GetWeakPtr()));

  // While we are waiting for the data, pretend we have work to do and return
  // true.
  return true;
}

bool PrinterJobHandler::HavePendingTasks() {
  return (job_check_pending_ || printer_update_pending_);
}

void PrinterJobHandler::ValidatePrintTicketFailed() {
  if (!shutting_down_) {
    LOG(ERROR) << "CP_CONNECTOR: Failed validating print ticket"
               << ", printer name: " << printer_info_.printer_name
               << ", job id: " << job_details_.job_id_;
    JobFailed(JOB_VALIDATE_TICKET_FAILED);
  }
}

void PrinterJobHandler::OnReceivePrinterCaps(
    bool succeeded,
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& caps_and_defaults) {
  printing::PrinterBasicInfo printer_info;
  if (printer_watcher_.get())
    printer_watcher_->GetCurrentPrinterInfo(&printer_info);

  std::string post_data;
  std::string mime_boundary = net::GenerateMimeMultipartBoundary();

  if (succeeded) {
    std::string caps_hash =
        base::MD5String(caps_and_defaults.printer_capabilities);
    if (caps_hash != printer_info_cloud_.caps_hash) {
      // Hashes don't match, we need to upload new capabilities (the defaults
      // go for free along with the capabilities)
      printer_info_cloud_.caps_hash = caps_hash;
      if (caps_and_defaults.caps_mime_type == kContentTypeJSON) {
        DCHECK(print_system_->UseCddAndCjt());
        net::AddMultipartValueForUpload(kUseCDD, "true", mime_boundary,
                                        std::string(), &post_data);
      }
      net::AddMultipartValueForUpload(kPrinterCapsValue,
          caps_and_defaults.printer_capabilities, mime_boundary,
          caps_and_defaults.caps_mime_type, &post_data);
      net::AddMultipartValueForUpload(kPrinterDefaultsValue,
          caps_and_defaults.printer_defaults, mime_boundary,
          caps_and_defaults.defaults_mime_type, &post_data);
      net::AddMultipartValueForUpload(kPrinterCapsHashValue,
          caps_hash, mime_boundary, std::string(), &post_data);
    }
  } else {
    LOG(ERROR) << "Failed to get printer caps and defaults"
               << ", printer name: " << printer_name;
  }

  std::string tags_hash = GetHashOfPrinterInfo(printer_info);
  if (tags_hash != printer_info_cloud_.tags_hash) {
    printer_info_cloud_.tags_hash = tags_hash;
    post_data += GetPostDataForPrinterInfo(printer_info, mime_boundary);
    // Remove all the existing proxy tags.
    std::string cp_tag_wildcard(kCloudPrintServiceProxyTagPrefix);
    cp_tag_wildcard += ".*";
    net::AddMultipartValueForUpload(kPrinterRemoveTagValue,
        cp_tag_wildcard, mime_boundary, std::string(), &post_data);
  }

  if (printer_info.printer_name != printer_info_.printer_name) {
    net::AddMultipartValueForUpload(kPrinterNameValue,
        printer_info.printer_name, mime_boundary, std::string(), &post_data);
  }
  if (printer_info.printer_description != printer_info_.printer_description) {
    net::AddMultipartValueForUpload(kPrinterDescValue,
      printer_info.printer_description, mime_boundary,
      std::string(), &post_data);
  }
  if (printer_info.printer_status != printer_info_.printer_status) {
    net::AddMultipartValueForUpload(
        kPrinterStatusValue, base::NumberToString(printer_info.printer_status),
        mime_boundary, std::string(), &post_data);
  }

  // Add local_settings with a current XMPP ping interval.
  if (printer_info_cloud_.pending_xmpp_timeout != 0) {
    DCHECK(kMinXmppPingTimeoutSecs <= printer_info_cloud_.pending_xmpp_timeout);
    net::AddMultipartValueForUpload(kPrinterLocalSettingsValue,
        base::StringPrintf(kUpdateLocalSettingsXmppPingFormat,
            printer_info_cloud_.current_xmpp_timeout),
        mime_boundary, std::string(), &post_data);
  }

  printer_info_ = printer_info;
  if (!post_data.empty()) {
    net::AddMultipartFinalDelimiterForUpload(mime_boundary, &post_data);
    std::string mime_type("multipart/form-data; boundary=");
    mime_type += mime_boundary;
    SetNextJSONHandler(&PrinterJobHandler::HandlePrinterUpdateResponse);
    request_ = CloudPrintURLFetcher::Create(kPartialTrafficAnnotation);
    request_->StartPostRequest(
        CloudPrintURLFetcher::REQUEST_UPDATE_PRINTER,
        GetUrlForPrinterUpdate(
            cloud_print_server_url_, printer_info_cloud_.printer_id),
        this,
        kCloudPrintAPIMaxRetryCount,
        mime_type,
        post_data,
        std::string());
  } else {
    // We are done here. Go to the Stop state
    VLOG(1) << "CP_CONNECTOR: Stopping printer job handler"
            << ", printer name: " << printer_name;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PrinterJobHandler::Stop, this));
  }
}

void PrinterJobHandler::DoPrint(const JobDetails& job_details,
                                const std::string& printer_name) {
  DCHECK(CurrentlyOnPrintThread());
  job_spooler_ = print_system_->CreateJobSpooler();
  UMA_HISTOGRAM_LONG_TIMES("CloudPrint.PrepareTime",
                           base::Time::Now() - job_start_time_);
  DCHECK(job_spooler_.get());

  base::string16 document_name =
      job_details.job_title_.empty()
          ? l10n_util::GetStringUTF16(IDS_DEFAULT_PRINT_DOCUMENT_TITLE)
          : base::UTF8ToUTF16(job_details.job_title_);

  document_name = printing::FormatDocumentTitleWithOwner(
      base::UTF8ToUTF16(job_details.job_owner_), document_name);

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.JobHandlerEvent",
                            JOB_HANDLER_START_SPOOLING, JOB_HANDLER_MAX);
  spooling_start_time_ = base::Time::Now();
  if (!job_spooler_->Spool(job_details.print_ticket_,
                           job_details.print_ticket_mime_type_,
                           job_details.print_data_file_path_,
                           job_details.print_data_mime_type_,
                           printer_name,
                           base::UTF16ToUTF8(document_name),
                           job_details.tags_,
                           this)) {
    OnJobSpoolFailed();
  }
}

bool PrinterJobHandler::CurrentlyOnPrintThread() const {
  return print_thread_.task_runner()->BelongsToCurrentThread();
}

}  // namespace cloud_print

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/job_status_updater.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/service/cloud_print/cloud_print_service_helpers.h"

namespace cloud_print {

namespace {

bool IsTerminalJobState(PrintJobStatus status) {
  return status == PRINT_JOB_STATUS_ERROR ||
         status == PRINT_JOB_STATUS_COMPLETED;
}

}  // namespace

JobStatusUpdater::JobStatusUpdater(
    const std::string& printer_name,
    const std::string& job_id,
    PlatformJobId local_job_id,
    const GURL& cloud_print_server_url,
    PrintSystem* print_system,
    Delegate* delegate,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
    : start_time_(base::Time::Now()),
      printer_name_(printer_name),
      job_id_(job_id),
      local_job_id_(local_job_id),
      cloud_print_server_url_(cloud_print_server_url),
      print_system_(print_system),
      delegate_(delegate),
      partial_traffic_annotation_(partial_traffic_annotation) {
  DCHECK(delegate_);
}

// Start checking the status of the local print job.
void JobStatusUpdater::UpdateStatus() {
  // It does not matter if we had already sent out an update and are waiting for
  // a response. This is a new update and we will simply cancel the old request
  // and send a new one.
  if (!stopped_) {
    bool need_update = false;
    // If the job has already been completed, we just need to update the server
    // with that status. The *only* reason we would come back here in that case
    // is if our last server update attempt failed.
    if (IsTerminalJobState(last_job_details_.status)) {
      need_update = true;
    } else {
      PrintJobDetails details;
      if (print_system_->GetJobDetails(printer_name_, local_job_id_,
                                       &details)) {
        if (details != last_job_details_) {
          last_job_details_ = details;
          need_update = true;
        }
      } else {
        // If GetJobDetails failed, the most likely case is that the job no
        // longer exists in the OS queue. We are going to assume it is done in
        // this case.
        last_job_details_.Clear();
        last_job_details_.status = PRINT_JOB_STATUS_COMPLETED;
        need_update = true;
      }
      UMA_HISTOGRAM_ENUMERATION("CloudPrint.NativeJobStatus",
                                last_job_details_.status, PRINT_JOB_STATUS_MAX);
    }
    if (need_update) {
      request_ = CloudPrintURLFetcher::Create(partial_traffic_annotation_);
      request_->StartGetRequest(
          CloudPrintURLFetcher::REQUEST_UPDATE_JOB,
          GetUrlForJobStatusUpdate(cloud_print_server_url_, job_id_,
                                   last_job_details_),
          this, kCloudPrintAPIMaxRetryCount);
    }
  }
}

void JobStatusUpdater::Stop() {
  request_ = nullptr;
  stopped_ = true;
  delegate_->OnJobCompleted(this);
}

// CloudPrintURLFetcher::Delegate implementation.
CloudPrintURLFetcher::ResponseAction JobStatusUpdater::HandleJSONData(
    const net::URLFetcher* source,
    const GURL& url,
    const base::Value& json_data,
    bool succeeded) {
  if (IsTerminalJobState(last_job_details_.status)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&JobStatusUpdater::Stop, this));
  }
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction JobStatusUpdater::OnRequestAuthError() {
  // We got an Auth error and have no idea how long it will take to refresh
  // auth information (may take forever). We'll drop current request and
  // propagate this error to the upper level. After auth issues will be
  // resolved, GCP connector will restart.
  delegate_->OnAuthError();
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

std::string JobStatusUpdater::GetAuthHeaderValue() {
  return GetCloudPrintAuthHeaderFromStore();
}

JobStatusUpdater::~JobStatusUpdater() {}

}  // namespace cloud_print

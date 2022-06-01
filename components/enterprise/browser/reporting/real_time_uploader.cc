// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_uploader.h"

#include <queue>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

// static
std::unique_ptr<RealTimeUploader> RealTimeUploader::Create(
    const std::string& dm_token,
    reporting::Destination destination,
    reporting::Priority priority) {
  auto uploader = base::WrapUnique(new RealTimeUploader(priority));
  uploader->CreateReportQueue(dm_token, destination);
  return uploader;
}

RealTimeUploader::RealTimeUploader(reporting::Priority priority)
    : report_priority_(priority) {}
RealTimeUploader::~RealTimeUploader() = default;

bool RealTimeUploader::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !!report_queue_;
}

void RealTimeUploader::CreateReportQueue(const std::string& dm_token,
                                         reporting::Destination destination) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto config = reporting::ReportQueueConfiguration::Create(
      dm_token, destination,
      base::BindRepeating([]() { return reporting::Status::StatusOK(); }));

  base::UmaHistogramEnumeration(
      "Enterprise.CBCMRealTimeReportQueueConfigurationCreation",
      config.status().code(), reporting::error::Code::MAX_VALUE);

  if (!config.ok()) {
    LOG(ERROR) << "Failed to create CBCM reporting queue config: "
               << config.status();
    return;
  }

  auto report_queue_creation_callback =
      base::BindPostTask(base::ThreadTaskRunnerHandle::Get(),
                         base::BindOnce(&RealTimeUploader::OnReportQueueCreated,
                                        weak_factory_.GetWeakPtr()));

  CreateReportQueueRequest(std::move(config),
                           std::move(report_queue_creation_callback));
}

void RealTimeUploader::Upload(
    std::unique_ptr<google::protobuf::MessageLite> report,
    EnqueueCallback callback) {
  DCHECK_NE(report_priority_, reporting::Priority::UNDEFINED_PRIORITY);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The |upload_closure| will be only called by the RealTimeUploader directly.
  auto upload_closure =
      base::BindOnce(&RealTimeUploader::UploadClosure, base::Unretained(this),
                     std::move(report), std::move(callback));
  if (!report_queue_) {
    pending_reports_.push(std::move(upload_closure));
    return;
  }

  std::move(upload_closure).Run();
}

void RealTimeUploader::CreateReportQueueRequest(
    reporting::StatusOr<std::unique_ptr<reporting::ReportQueueConfiguration>>
        config,
    reporting::ReportQueueProvider::CreateReportQueueCallback callback) {
#if !BUILDFLAG(IS_IOS)
  reporting::ReportQueueProvider::CreateQueue(std::move(config.ValueOrDie()),
                                              std::move(callback));
#else
  NOTREACHED();
#endif  // !BUILDFLAG(IS_IOS)
}

void RealTimeUploader::OnReportQueueCreated(
    reporting::ReportQueueProvider::CreateReportQueueResponse
        create_report_queue_response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramEnumeration("Enterprise.CBCMRealTimeReportQueueCreation",
                                create_report_queue_response.status().code(),
                                reporting::error::Code::MAX_VALUE);

  if (!create_report_queue_response.ok()) {
    // TODO(crbug.com/1192292): Retry on unavailable error, and collect metrics
    // on failure rate.
    LOG(ERROR) << "Failed to create CBCM reporting queue. "
               << create_report_queue_response.status();
    return;
  }
  report_queue_ = std::move(create_report_queue_response.ValueOrDie());

  while (!pending_reports_.empty()) {
    std::move(pending_reports_.front()).Run();
    pending_reports_.pop();
  }
}

void RealTimeUploader::UploadClosure(
    std::unique_ptr<const google::protobuf::MessageLite> report,
    EnqueueCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(report_queue_);
  report_queue_->Enqueue(
      std::move(report), report_priority_,
      base::BindPostTask(
          base::ThreadTaskRunnerHandle::Get(),
          base::BindOnce(&RealTimeUploader::OnReportEnqueued,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void RealTimeUploader::OnReportEnqueued(EnqueueCallback callback,
                                        reporting::Status status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramEnumeration("Enterprise.CBCMRealTimeReportEnqueue",
                                status.code(),
                                reporting::error::Code::MAX_VALUE);
  std::move(callback).Run(status.ok());
}

}  // namespace enterprise_reporting

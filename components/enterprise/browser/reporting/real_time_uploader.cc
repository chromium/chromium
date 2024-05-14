// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_uploader.h"

#include <queue>
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

// static
std::unique_ptr<RealTimeUploader> RealTimeUploader::Create(
    const std::string& dm_token,
    reporting::Destination destination,
    reporting::Priority priority) {
  auto uploader = base::WrapUnique(new RealTimeUploader(priority));
  // Create report queue outside ctor so that it can be skipped in unrelated
  // unittests.
  uploader->CreateReportQueue(dm_token, destination);
  return uploader;
}

RealTimeUploader::RealTimeUploader(reporting::Priority priority)
    : report_queue_(nullptr,
                    base::OnTaskRunnerDeleter(
                        base::SequencedTaskRunner::GetCurrentDefault())),
      report_priority_(priority) {}

RealTimeUploader::~RealTimeUploader() = default;

void RealTimeUploader::Upload(
    std::unique_ptr<google::protobuf::MessageLite> report,
    EnqueueCallback callback) {
  DCHECK_NE(report_priority_, reporting::Priority::UNDEFINED_PRIORITY);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!report_queue_) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  report_queue_->Enqueue(
      std::move(report), report_priority_,
      base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindOnce(&RealTimeUploader::OnReportEnqueued,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

reporting::ReportQueue* RealTimeUploader::GetReportQueue() const {
  return report_queue_.get();
}

void RealTimeUploader::CreateReportQueue(const std::string& dm_token,
                                         reporting::Destination destination) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if !BUILDFLAG(IS_IOS)
  // Not using ReportQueueFactory as we need to provide DM token manually.
  auto config = reporting::ReportQueueConfiguration::Create(
      dm_token, destination,
      base::BindRepeating([]() { return reporting::Status::StatusOK(); }));

  if (!config.has_value()) {
    // No special handler as we never record reporting queue config creation
    // failure.
    LOG(ERROR) << "Failed to create CBCM reporting queue config: "
               << config.error();
    return;
  }

  auto report_queue = reporting::ReportQueueProvider::CreateSpeculativeQueue(
      std::move(config.value()));
  if (!report_queue.has_value()) {
    // No special handler as we never record reporting queue creation failure.
    LOG(ERROR) << "Failed to create CBCM reporting queue. "
               << report_queue.error();
    return;
  }
  report_queue_ = std::move(report_queue.value());
#else
  NOTREACHED_IN_MIGRATION();
#endif  // !BUILDFLAG(IS_IOS)
}

void RealTimeUploader::OnReportEnqueued(EnqueueCallback callback,
                                        reporting::Status status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::UmaHistogramEnumeration("Enterprise.CBCMRealTimeReportEnqueue",
                                status.code(),
                                reporting::error::Code::MAX_VALUE);
  LOG_IF(ERROR, !status.ok()) << "Failed to enqueue a request: " << status;
  std::move(callback).Run(status.ok());
}

}  // namespace enterprise_reporting

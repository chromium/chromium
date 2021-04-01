// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_uploader.h"

#include "base/bind_post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

RealTimeUploader::RealTimeUploader() = default;
RealTimeUploader::~RealTimeUploader() = default;

bool RealTimeUploader::IsEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !!report_queue_;
}

// static
std::unique_ptr<RealTimeUploader> RealTimeUploader::Create(
    const std::string& dm_token,
    reporting::Destination destination) {
  auto uploader = std::make_unique<RealTimeUploader>();
  uploader->CreateReportQueue(dm_token, destination);
  return uploader;
}

void RealTimeUploader::CreateReportQueue(const std::string& dm_token,
                                         reporting::Destination destination) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto config = reporting::ReportQueueConfiguration::Create(
      dm_token, destination,
      base::BindRepeating([]() { return reporting::Status::StatusOK(); }));

  if (!config.ok()) {
    LOG(ERROR) << "Failed to create CBCM reporting queue config: "
               << config.status();
    return;
  }

  auto report_queue_creation_callback = base::BindOnce(
      &RealTimeUploader::OnReportQueueCreated, weak_factory_.GetWeakPtr());

  CreateReportQueueRequest(
      std::move(config),
      base::BindPostTask(base::ThreadTaskRunnerHandle::Get(),
                         std::move(report_queue_creation_callback)));
}

void RealTimeUploader::CreateReportQueueRequest(
    reporting::StatusOr<std::unique_ptr<reporting::ReportQueueConfiguration>>
        config,
    reporting::ReportQueueProvider::CreateReportQueueCallback callback) {
  reporting::ReportQueueProvider::CreateQueue(std::move(config.ValueOrDie()),
                                              std::move(callback));
}

void RealTimeUploader::OnReportQueueCreated(
    reporting::ReportQueueProvider::CreateReportQueueResponse
        create_report_queue_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!create_report_queue_response.ok()) {
    // TODO(crbug.com/1192292): Retry on unavailable error, and collect metrics
    // on failure rate.
    LOG(ERROR) << "Failed to create CBCM reporting queue. "
               << create_report_queue_response.status();
    return;
  }
  report_queue_ = std::move(create_report_queue_response.ValueOrDie());
}
}  // namespace enterprise_reporting

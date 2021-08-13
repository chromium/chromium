// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_factory.h"

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/util/backoff_settings.h"
#include "net/base/backoff_entry.h"

// TODO(1229994, marcgrimme) remove the logs after enterprise customer
// feedback has been conculded.)
#define LOG_WITH_STATUS(LEVEL, MESSAGE, STATUS) \
  VLOG(LEVEL) << MESSAGE << " status=" << STATUS.status();

namespace reporting {
// TODO(1229988, marcgrimme): refactor the below logic according to
//    the suggestions in the ticket.

// static
void ReportQueueFactory::Create(base::StringPiece dm_token_value,
                                const Destination destination,
                                SuccessCallback success_cb) {
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  auto config_result = ReportQueueConfiguration::Create(
      std::string(dm_token_value.data()), destination,
      base::BindRepeating([]() { return Status::StatusOK(); }));
  if (!config_result.ok()) {
    LOG_WITH_STATUS(1, "ReportQueueConfiguration is invalid.", config_result);
    return;
  }

  // Asynchronously create and try to set ReportQueue.
  auto try_set_cb = CreateTrySetCallback(
      dm_token_value, destination, std::move(success_cb), GetBackoffEntry());
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(ReportQueueProvider::CreateQueue,
                                std::move(config_result.ValueOrDie()),
                                std::move(try_set_cb)));
}

// static
std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
ReportQueueFactory::CreateSpeculativeReportQueue(
    base::StringPiece dm_token_value,
    const Destination destination) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());

  auto config_result = ::reporting::ReportQueueConfiguration::Create(
      dm_token_value, destination,
      base::BindRepeating([]() { return ::reporting::Status::StatusOK(); }));

  if (!config_result.ok()) {
    DVLOG(1)
        << "Cannot initialize report queue. Invalid ReportQueueConfiguration: "
        << config_result.status();
    return std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
        nullptr,
        base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  }

  auto speculative_queue_result =
      ::reporting::ReportQueueProvider::CreateSpeculativeQueue(
          std::move(config_result.ValueOrDie()));
  if (!speculative_queue_result.ok()) {
    DVLOG(1) << "Failed to create speculative queue";
    return std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
        nullptr,
        base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  }

  return std::move(speculative_queue_result.ValueOrDie());
}

ReportQueueFactory::TrySetReportQueueCallback
ReportQueueFactory::CreateTrySetCallback(
    base::StringPiece dm_token_value,
    const Destination destination,
    SuccessCallback success_cb,
    std::unique_ptr<net::BackoffEntry> backoff_entry) {
  return base::BindPostTask(
      base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(&ReportQueueFactory::TrySetReportQueue,
                     std::move(success_cb)));
}

// static
void ReportQueueFactory::TrySetReportQueue(
    SuccessCallback success_cb,
    StatusOr<std::unique_ptr<ReportQueue>> report_queue_result) {
  if (!report_queue_result.ok()) {
    LOG_WITH_STATUS(1, "ReportQueue could not be created.",
                    report_queue_result);
    return;
  }
  std::move(success_cb).Run(std::move(report_queue_result.ValueOrDie()));
}
}  // namespace reporting

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/util/backoff_settings.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "net/base/backoff_entry.h"

#define LOG_WITH_STATUS(LEVEL, MESSAGE, STATUS) \
  VLOG(LEVEL) << MESSAGE << " status=" << STATUS.error();

namespace reporting {

// static
void ReportQueueFactory::Create(
    ReportQueueConfiguration::Builder config_builder,
    SuccessCallback done_cb) {
  auto config_result = config_builder.Build();
  if (!config_result.has_value()) {
    LOG_WITH_STATUS(1, "ReportQueueConfiguration is invalid.", config_result);
    return;
  }

  // Asynchronously create and try to set ReportQueue.
  auto try_set_cb = CreateTrySetCallback(std::move(done_cb), GetBackoffEntry());
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(ReportQueueProvider::CreateQueue,
                     std::move(config_result.value()), std::move(try_set_cb)));
}

//  static
std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>
ReportQueueFactory::CreateSpeculativeReportQueue(
    ReportQueueConfiguration::Builder config_builder) {
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  auto config_result = config_builder.Build();
  if (!config_result.has_value()) {
    DVLOG(1)
        << "Cannot initialize report queue. Invalid ReportQueueConfiguration: "
        << config_result.error();
    return std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>(
        nullptr, base::OnTaskRunnerDeleter(
                     base::SequencedTaskRunner::GetCurrentDefault()));
  }

  auto speculative_queue_result = ReportQueueProvider::CreateSpeculativeQueue(
      std::move(config_result.value()));
  if (!speculative_queue_result.has_value()) {
    DVLOG(1) << "Failed to create speculative queue: "
             << speculative_queue_result.error();
    return std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>(
        nullptr, base::OnTaskRunnerDeleter(
                     base::SequencedTaskRunner::GetCurrentDefault()));
  }

  return std::move(speculative_queue_result.value());
}

// static
void ReportQueueFactory::Create(
    EventType event_type,
    Destination destination,
    SuccessCallback success_cb,
    std::unique_ptr<RateLimiterInterface> rate_limiter,
    int64_t reserved_space) {
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  ReportQueueFactory::Create(
      ReportQueueConfiguration::Create({.event_type = event_type,
                                        .destination = destination,
                                        .reserved_space = reserved_space})
          .SetRateLimiter(std::move(rate_limiter)),
      std::move(success_cb));
}

// static
std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>
ReportQueueFactory::CreateSpeculativeReportQueue(
    EventType event_type,
    Destination destination,
    std::unique_ptr<RateLimiterInterface> rate_limiter,
    int64_t reserved_space) {
  return ReportQueueFactory::CreateSpeculativeReportQueue(
      ReportQueueConfiguration::Create({.event_type = event_type,
                                        .destination = destination,
                                        .reserved_space = reserved_space})
          .SetRateLimiter(std::move(rate_limiter)));
}

ReportQueueFactory::TrySetReportQueueCallback
ReportQueueFactory::CreateTrySetCallback(
    SuccessCallback success_cb,
    std::unique_ptr<::net::BackoffEntry> backoff_entry) {
  return base::BindPostTaskToCurrentDefault(base::BindOnce(
      &ReportQueueFactory::TrySetReportQueue, std::move(success_cb)));
}

// static
void ReportQueueFactory::TrySetReportQueue(
    SuccessCallback success_cb,
    StatusOr<std::unique_ptr<ReportQueue>> report_queue_result) {
  if (!report_queue_result.has_value()) {
    LOG_WITH_STATUS(1, "ReportQueue could not be created.",
                    report_queue_result);
    return;
  }
  std::move(success_cb).Run(std::move(report_queue_result.value()));
}
}  // namespace reporting

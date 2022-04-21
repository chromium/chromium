// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_impl.h"

#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

void ReportQueueImpl::Create(
    std::unique_ptr<ReportQueueConfiguration> config,
    scoped_refptr<StorageModuleInterface> storage,
    base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)> cb) {
  std::move(cb).Run(base::WrapUnique<ReportQueueImpl>(
      new ReportQueueImpl(std::move(config), storage)));
}

ReportQueueImpl::~ReportQueueImpl() = default;

ReportQueueImpl::ReportQueueImpl(
    std::unique_ptr<ReportQueueConfiguration> config,
    scoped_refptr<StorageModuleInterface> storage)
    : config_(std::move(config)),
      storage_(storage),
      sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits())) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ReportQueueImpl::AddRecord(base::StringPiece record,
                                Priority priority,
                                EnqueueCallback callback) const {
  const Status status = config_->CheckPolicy();
  if (!status.ok()) {
    std::move(callback).Run(status);
    return;
  }

  if (priority == Priority::UNDEFINED_PRIORITY) {
    std::move(callback).Run(
        Status(error::INVALID_ARGUMENT, "Priority must be defined"));
    return;
  }

  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ReportQueueImpl::SendRecordToStorage,
                                base::Unretained(this), std::string(record),
                                priority, std::move(callback)));
}

void ReportQueueImpl::SendRecordToStorage(base::StringPiece record_data,
                                          Priority priority,
                                          EnqueueCallback callback) const {
  storage_->AddRecord(priority, AugmentRecord(record_data),
                      std::move(callback));
}

Record ReportQueueImpl::AugmentRecord(base::StringPiece record_data) const {
  Record record;
  record.set_data(std::string(record_data));
  record.set_destination(config_->destination());

  // record with no DM token is assumed to be associated with device DM token
  if (!config_->dm_token().empty()) {
    record.set_dm_token(config_->dm_token());
  }

  // Calculate timestamp in microseconds - to match Spanner expectations.
  const int64_t time_since_epoch_us =
      base::Time::Now().ToJavaTime() * base::Time::kMicrosecondsPerMillisecond;
  record.set_timestamp_us(time_since_epoch_us);
  return record;
}

void ReportQueueImpl::Flush(Priority priority, FlushCallback callback) {
  storage_->Flush(priority, std::move(callback));
}

base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>
ReportQueueImpl::PrepareToAttachActualQueue() const {
  NOTREACHED();
  return base::BindOnce(
      [](StatusOr<std::unique_ptr<ReportQueue>>) { NOTREACHED(); });
}

// static
std::unique_ptr<SpeculativeReportQueueImpl, base::OnTaskRunnerDeleter>
SpeculativeReportQueueImpl::Create() {
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()});
  return std::unique_ptr<SpeculativeReportQueueImpl, base::OnTaskRunnerDeleter>(
      new SpeculativeReportQueueImpl(sequenced_task_runner),
      base::OnTaskRunnerDeleter(sequenced_task_runner));
}

SpeculativeReportQueueImpl::SpeculativeReportQueueImpl(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : sequenced_task_runner_(sequenced_task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SpeculativeReportQueueImpl::~SpeculativeReportQueueImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SpeculativeReportQueueImpl::Flush(Priority priority,
                                       FlushCallback callback) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](Priority priority, FlushCallback callback,
             base::WeakPtr<SpeculativeReportQueueImpl> self) {
            if (!self) {
              std::move(callback).Run(
                  Status(error::UNAVAILABLE, "Queue has been destructed"));
              return;
            }
            DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
            if (!self->report_queue_) {
              std::move(callback).Run(Status(error::FAILED_PRECONDITION,
                                             "ReportQueue is not ready yet."));
              return;
            }
            self->report_queue_->Flush(priority, std::move(callback));
          },
          priority, std::move(callback), weak_ptr_factory_.GetWeakPtr()));
}

void SpeculativeReportQueueImpl::AddRecord(base::StringPiece record,
                                           Priority priority,
                                           EnqueueCallback callback) const {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeculativeReportQueueImpl::MaybeEnqueueRecord,
                     weak_ptr_factory_.GetWeakPtr(), std::string(record),
                     priority, std::move(callback)));
}

void SpeculativeReportQueueImpl::MaybeEnqueueRecord(
    base::StringPiece record,
    Priority priority,
    EnqueueCallback callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!report_queue_) {
    // Queue is not ready yet, store the record in the memory
    // queue.
    pending_records_.emplace(record, priority);
    std::move(callback).Run(Status::StatusOK());
    return;
  }
  // Queue is ready. If memory queue is empty, just forward the
  // record.
  if (pending_records_.empty()) {
    report_queue_->Enqueue(record, priority, std::move(callback));
    return;
  }
  // If memory queue is not empty, attach the new record at the
  // end and initiate enqueuing of everything from there.
  pending_records_.emplace(record, priority);
  EnqueuePendingRecords(std::move(callback));
}

void SpeculativeReportQueueImpl::EnqueuePendingRecords(
    EnqueueCallback callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(report_queue_);
  if (pending_records_.empty()) {
    std::move(callback).Run(Status::StatusOK());
    return;
  }

  std::string record(pending_records_.front().first);
  Priority priority = pending_records_.front().second;
  pending_records_.pop();
  if (pending_records_.empty()) {
    // Last of the pending records.
    report_queue_->Enqueue(record, priority, std::move(callback));
    return;
  }
  report_queue_->Enqueue(
      record, priority,
      base::BindPostTask(
          sequenced_task_runner_,
          base::BindOnce(
              [](base::WeakPtr<const SpeculativeReportQueueImpl> self,
                 EnqueueCallback callback, Status status) {
                if (!status.ok()) {
                  std::move(callback).Run(status);
                  return;
                }
                if (!self) {
                  std::move(callback).Run(
                      Status(error::UNAVAILABLE, "Queue has been destructed"));
                  return;
                }
                self->sequenced_task_runner_->PostTask(
                    FROM_HERE,
                    base::BindOnce(
                        &SpeculativeReportQueueImpl::EnqueuePendingRecords,
                        self, std::move(callback)));
              },
              weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>
SpeculativeReportQueueImpl::PrepareToAttachActualQueue() const {
  return base::BindPostTask(
      sequenced_task_runner_,
      base::BindOnce(
          [](base::WeakPtr<SpeculativeReportQueueImpl> speculative_queue,
             StatusOr<std::unique_ptr<ReportQueue>> actual_queue_result) {
            if (!speculative_queue) {
              return;  // Speculative queue was destructed in a meantime.
            }
            if (!actual_queue_result.ok()) {
              return;  // Actual queue creation failed.
            }
            // Set actual queue for the speculative queue to use
            // (asynchronously).
            speculative_queue->AttachActualQueue(
                std::move(actual_queue_result.ValueOrDie()));
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void SpeculativeReportQueueImpl::AttachActualQueue(
    std::unique_ptr<ReportQueue> actual_queue) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<SpeculativeReportQueueImpl> self,
             std::unique_ptr<ReportQueue> actual_queue) {
            if (!self) {
              return;
            }
            DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
            if (self->report_queue_) {
              // Already attached, do nothing.
              return;
            }
            self->report_queue_ = std::move(actual_queue);
            if (!self->pending_records_.empty()) {
              self->EnqueuePendingRecords(
                  base::BindOnce([](Status enqueue_status) {
                    if (!enqueue_status.ok()) {
                      LOG(ERROR) << "Pending records failed to enqueue, status="
                                 << enqueue_status;
                    }
                  }));
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(actual_queue)));
}

}  // namespace reporting

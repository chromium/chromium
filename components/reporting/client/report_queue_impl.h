// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_IMPL_H_
#define COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_IMPL_H_

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/wrapped_rate_limiter.h"

namespace reporting {

// A |ReportQueueImpl| is configured with a |ReportQueueConfiguration|.  A
// |ReportQueueImpl| allows a user to |Enqueue| a message for delivery to a
// handler specified by the |Destination| held by the provided
// |ReportQueueConfiguration|. |ReportQueueImpl| handles scheduling storage and
// delivery.
//
// ReportQueues are not meant to be created directly, instead use the
// reporting::ReportQueueProvider::CreateQueue(...) function. See the
// comments for reporting::ReportingClient for example usage.
//
// Enqueue can also be used with a |base::Value| or |std::string|.
class ReportQueueImpl : public ReportQueue {
 public:
  // Factory
  static void Create(
      std::unique_ptr<ReportQueueConfiguration> config,
      scoped_refptr<StorageModuleInterface> storage,
      base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)> cb);

  ReportQueueImpl(const ReportQueueImpl& other) = delete;
  ReportQueueImpl& operator=(const ReportQueueImpl& other) = delete;
  ~ReportQueueImpl() override;

  void Flush(Priority priority, FlushCallback callback) override;

  // Dummy implementation for a regular queue.
  [[nodiscard]] base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>
  PrepareToAttachActualQueue() const override;

  // ReportQueue:
  Destination GetDestination() const override;

 protected:
  ReportQueueImpl(std::unique_ptr<ReportQueueConfiguration> config,
                  scoped_refptr<StorageModuleInterface> storage);

 private:
  void AddProducedRecord(RecordProducer record_producer,
                         Priority priority,
                         EnqueueCallback callback) const override;

  const std::unique_ptr<ReportQueueConfiguration> config_;
  const scoped_refptr<StorageModuleInterface> storage_;
};

class SpeculativeReportQueueImpl : public ReportQueue {
 public:
  // Factory method returns a smart pointer with on-thread deleter.
  static std::unique_ptr<SpeculativeReportQueueImpl, base::OnTaskRunnerDeleter>
  Create(const SpeculativeConfigSettings& config_settings);

  SpeculativeReportQueueImpl(const SpeculativeReportQueueImpl& other) = delete;
  SpeculativeReportQueueImpl& operator=(
      const SpeculativeReportQueueImpl& other) = delete;
  ~SpeculativeReportQueueImpl() override;

  // Forwards |Flush| to |ReportQueue|, if already created.
  // Returns with failure otherwise.
  void Flush(Priority priority, FlushCallback callback) override;

  // Provides a callback to attach initialized actual queue to the speculative
  // queue.
  [[nodiscard]] base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>
  PrepareToAttachActualQueue() const override;

  // ReportQueue:
  Destination GetDestination() const override;

 private:
  // Moveable, non-copyable struct holding a pending record producer for the
  // |pending_record_producers_| queue below.
  struct PendingRecordProducer {
    PendingRecordProducer(RecordProducer producer,
                          EnqueueCallback callback,
                          Priority priority);
    PendingRecordProducer(PendingRecordProducer&& other);
    PendingRecordProducer& operator=(PendingRecordProducer&& other);
    ~PendingRecordProducer();

    RecordProducer record_producer;
    EnqueueCallback record_callback;
    Priority record_priority;
  };

  // Private constructor, used by the factory method  only.
  explicit SpeculativeReportQueueImpl(
      const SpeculativeConfigSettings& config_settings,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  // Forwards |AddProducedRecord| to |ReportQueue|, if already created.
  // Records the record internally otherwise.
  void AddProducedRecord(RecordProducer record_producer,
                         Priority priority,
                         EnqueueCallback callback) const override;

  // Substitutes actual queue to the speculative, when ready.
  // Initiates processesing of all pending records.
  void AttachActualQueue(
      StatusOr<std::unique_ptr<ReportQueue>> status_or_actual_queue);

  // Enqueues head of the |pending_record_producers_| and reapplies for the rest
  // of it.
  void EnqueuePendingRecordProducers() const;

  // Purges all |pending_record_producers_| with error.
  void PurgePendingProducers(Status status) const;

  // Optionally enqueues |record_producer| (owned) to actual queue, if ready.
  // Otherwise adds it to the end of |pending_record_producers_|.
  void MaybeEnqueueRecordProducer(Priority priority,
                                  EnqueueCallback callback,
                                  RecordProducer record_producer) const;

  // Task runner that protects |report_queue_| and |pending_record_producers_|
  // and allows to synchronize the initialization.
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Actual |ReportQueue| once successfully created (immutable after that).
  std::optional<std::unique_ptr<ReportQueue>> actual_report_queue_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Queue of the pending record producers, collected before actual queue has
  // been created. Declared 'mutable', because it is accessed by 'const'
  // methods.
  mutable std::queue<PendingRecordProducer> pending_record_producers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Report queue configuration settings that are supposed to be identical to
  // the one configured with the `actual_report_queue_`.
  const SpeculativeConfigSettings config_settings_;

  // Weak pointer factory.
  base::WeakPtrFactory<SpeculativeReportQueueImpl> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_IMPL_H_

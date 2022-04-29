// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_IMPL_H_
#define COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_IMPL_H_

#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

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

  ~ReportQueueImpl() override;
  ReportQueueImpl(const ReportQueueImpl& other) = delete;
  ReportQueueImpl& operator=(const ReportQueueImpl& other) = delete;

  void Flush(Priority priority, FlushCallback callback) override;

  // Dummy implementation for a regular queue.
  [[nodiscard]] base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>
  PrepareToAttachActualQueue() const override;

 protected:
  ReportQueueImpl(std::unique_ptr<ReportQueueConfiguration> config,
                  scoped_refptr<StorageModuleInterface> storage);

 private:
  void AddRecord(base::StringPiece record,
                 Priority priority,
                 EnqueueCallback callback) const override;

  void SendRecordToStorage(base::StringPiece record,
                           Priority priority,
                           EnqueueCallback callback) const;

  [[nodiscard]] reporting::Record AugmentRecord(
      base::StringPiece record_data) const;

  std::unique_ptr<ReportQueueConfiguration> config_;
  scoped_refptr<StorageModuleInterface> storage_;
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

class SpeculativeReportQueueImpl : public ReportQueue {
 public:
  ~SpeculativeReportQueueImpl() override;

  // Factory method returns a smart pointer with on-thread deleter.
  static std::unique_ptr<SpeculativeReportQueueImpl, base::OnTaskRunnerDeleter>
  Create();

  // Forwards |Flush| to |ReportQueue|, if already created.
  // Returns with failure otherwise.
  void Flush(Priority priority, FlushCallback callback) override;

  // Provides a callback to attach initialized actual queue to the speculative
  // queue.
  [[nodiscard]] base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>
  PrepareToAttachActualQueue() const override;

  // Substitutes actual queue to the speculative, when ready.
  // Initiates processesing of all pending records.
  void AttachActualQueue(std::unique_ptr<ReportQueue> actual_queue);

 protected:
  // Forwards |AddRecord| to |ReportQueue|, if already created.
  // Records the record internally otherwise.
  void AddRecord(base::StringPiece record,
                 Priority priority,
                 EnqueueCallback callback) const override;

 private:
  // Private constructor, used by the factory method  only.
  explicit SpeculativeReportQueueImpl(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  // Enqueues head of the |pending_records_| and reapplies for the rest of it.
  void EnqueuePendingRecords(EnqueueCallback callback) const;

  // Optionally enqueues |record| to actual queue, if ready.
  // Otherwise adds it to the end of |pending_records_|.
  void MaybeEnqueueRecord(base::StringPiece record,
                          Priority priority,
                          EnqueueCallback callback) const;

  // Task runner that protects |report_queue_| and |pending_records_|
  // and allows to synchronize the initialization.
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Actual |ReportQueue|, once created.
  std::unique_ptr<ReportQueue> report_queue_;

  // Queue of the pending records, collected before actual queue has been
  // created. Declared 'mutable', because it is accessed by 'const' methods.
  mutable std::queue<std::pair<std::string /*record*/, Priority /*priority*/>>
      pending_records_;

  // Weak pointer factory.
  base::WeakPtrFactory<SpeculativeReportQueueImpl> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_IMPL_H_

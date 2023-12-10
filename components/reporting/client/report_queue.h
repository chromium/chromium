// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_H_
#define COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_H_

#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace reporting {

// A |ReportQueue| is not meant to be created directly, instead it is
// instantiated by |ReportQueueProvider|. |ReportQueue| allows a user
// to |Enqueue| a message for delivery to a handler specified by the
// |Destination| held by the provided |ReportQueueConfiguration|.
// |ReportQueue| implementation handles scheduling storage and
// delivery.
// Enqueue can also be used with a |base::Value| or |std::string|.
//
// Example Usage:
// void SendMessage(google::protobuf::ImportantMessage important_message,
//                  reporting::ReportQueue::EnqueueCallback done_cb) {
//   // Create configuration.
//   StatusOr<reporting::ReportQueueConfiguration> config_result =
//      reporting::ReportQueueConfiguration::Create({...}).Set...().Build();
//   // Bail out if configuration failed to create.
//   if (!config_result.has_value()) {
//     std::move(done_cb).Run(config_result.error());
//     return;
//   }
//   // Asynchronously instantiate ReportQueue.
//   base::ThreadPool::PostTask(
//       FROM_HERE,
//       base::BindOnce(
//           [](google::protobuf::ImportantMessage important_message,
//              reporting::ReportQueue::EnqueueCallback done_cb,
//              std::unique_ptr<reporting::ReportQueueConfiguration> config) {
//             reporting::ReportQueueProvider::CreateQueue(
//                 std::move(config),
//                 base::BindOnce(
//                     [](google::protobuf::ImportantMessage important_message,
//                        reporting::ReportQueue::EnqueueCallback done_cb,
//                        reporting::StatusOr<std::unique_ptr<
//                            reporting::ReportQueue>> report_queue_result) {
//                       // Bail out if queue failed to create.
//                       if (!report_queue_result.has_value()) {
//                         std::move(done_cb).Run(report_queue_result.error());
//                         return;
//                       }
//                       // Queue created successfully, enqueue the message.
//                       report_queue_result.value()->Enqueue(
//                           std::move(important_message), std::move(done_cb));
//                     },
//                     std::move(important_message), std::move(done_cb)));
//           },
//           std::move(important_message), std::move(done_cb),
//           std::move(config_result.value())));
// }
//
// |SpeculativeReportQueueImpl| is an extension to |ReportQueue| which allows
// to speculatively enqueue records before the actual |ReportQueue| is created
// (which may be delayed by inability to initialize |ReportClient|).
// Instantiated by |ReportQueueProvider| and can be used anywhere |ReportQueue|
// fits. Note however, that records enqueued before actual |ReportQueue|
// is ready may be lost, e.g. if the machine reboots, so for the records
// that need to be definiately recorded |ReportQueue| is preferable.
//
// Example Usage:
// void SendMessage(google::protobuf::LessImportantMessage
// less_important_message,
//                  reporting::ReportQueue::EnqueueCallback done_cb) {
//   // Create configuration.
//   StatusOr<reporting::ReportQueueConfiguration> config_result =
//      reporting::ReportQueueConfiguration::Create({...}).Set...().Build();
//   // Bail out if configuration failed to create.
//   if (!config_result.has_value()) {
//     std::move(done_cb).Run(config_result.error());
//     return;
//   }
//   // Synchronously instantiate SpeculativeReportQueueImpl, returning it as
//   // ReportQueue still.
//   auto report_queue_result =
//       reporting::ReportQueueProvider::CreateSpeculativeQueue(
//           std::move(config));
//   if (!report_queue_result.has_value()) {
//     std::move(done_cb).Run(config_result.error());
//     return;
//   }
//   // Enqueue event (store it in memory only until the actual queue is
//   // created).
//   report_queue_result.value()->Enqueue(
//       std::move(less_important_message), std::move(done_cb));
// }

class ReportQueue {
 public:
  // A callback to asynchronously generate data to be added to |Storage|.
  using RecordProducer = base::OnceCallback<StatusOr<std::string>()>;

  // An EnqueueCallback is called on the completion of any |Enqueue| call.
  using EnqueueCallback = base::OnceCallback<void(Status)>;

  // A FlushCallback is called on the completion of |Flush| call.
  using FlushCallback = base::OnceCallback<void(Status)>;

  // Speculative report queue config settings used during its instantiation.
  struct SpeculativeConfigSettings {
    Destination destination = Destination::UNDEFINED_DESTINATION;
  };

  // Enqueue metrics name.
  static constexpr char kEnqueueMetricsName[] =
      "Browser.ERP.EventEnqueueResult";

  // Enqueue failed reporting destination metrics name.
  static constexpr char kEnqueueFailedDestinationMetricsName[] =
      "Browser.ERP.EnqueueFailureDestination";

  // Enqueue success reporting destination metrics name.
  static constexpr char kEnqueueSuccessDestinationMetricsName[] =
      "Browser.ERP.EnqueueSuccessDestination";

  virtual ~ReportQueue();

  // Enqueue asynchronously stores and delivers a record.  The |callback| will
  // be called on any errors. If storage is successful |callback| will be called
  // with an OK status.
  //
  // |priority| will Enqueue the record to the specified Priority queue.
  //
  // The current destinations have the following data requirements:
  // (destination : requirement)
  // UPLOAD_EVENTS : UploadEventsRequest
  //
  // |record| string (owned) will be sent with no conversion.
  void Enqueue(std::string record,
               Priority priority,
               EnqueueCallback callback) const;

  // |record| as a dictionary (owned) will be converted to a JSON string with
  // base::JsonWriter::Write.
  void Enqueue(base::Value::Dict record,
               Priority priority,
               EnqueueCallback callback) const;

  // |record| as a protobuf (owned) will be converted to a string with
  // SerializeToString(). The handler is responsible for converting the record
  // back to a proto with a ParseFromString() call.
  void Enqueue(std::unique_ptr<const google::protobuf::MessageLite> record,
               Priority priority,
               EnqueueCallback callback) const;

  // Initiates upload of collected records according to the priority.
  // Called usually for a queue with an infinite or very large upload period.
  // Multiple |Flush| calls can safely run in parallel.
  // Returns error if cannot start upload.
  virtual void Flush(Priority priority, FlushCallback callback) = 0;

  // Prepares a callback to attach actual queue to the speculative.
  // Implemented only in SpeculativeReportQueue, CHECKs in a regular one.
  [[nodiscard]] virtual base::OnceCallback<
      void(StatusOr<std::unique_ptr<ReportQueue>>)>
  PrepareToAttachActualQueue() const = 0;

  // Returns the reporting destination used to configure the report queue.
  virtual Destination GetDestination() const = 0;

 private:
  // Allow SpeculativeReportQueue access to |AddProducedRecord|.
  friend class SpeculativeReportQueueImpl;

  // Invokes |record_producer| and posts resulting data to the queue storage.
  // |record_producer| is expected to be called asynchronously.
  // Should only be used within ReportQueue implementation and its derivatives.
  virtual void AddProducedRecord(RecordProducer record_producer,
                                 Priority priority,
                                 EnqueueCallback callback) const = 0;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_H_

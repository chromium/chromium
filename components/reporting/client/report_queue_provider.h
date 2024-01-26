// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_H_
#define COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_H_

#include <memory>
#include <queue>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

BASE_DECLARE_FEATURE(kEncryptedReportingPipeline);

// ReportQueueProvider acts a single point for instantiating
// |reporting::ReportQueue|s. By performing initialization atomically it ensures
// that all ReportQueues are created with the same global settings.
//
// In order to utilize the ReportQueueProvider the EncryptedReportingPipeline
// feature must be turned on using --enable-features=EncryptedReportingPipeline.
//
// `ReportQueueProvider` must be created for a specific configuration - Chrome
// (for ChromeOS and for other OSes), other ChromeOS executables. It is then
// registered and can be accessed through static method
// `ReportQueueProvider::GetInstance()`.
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
//   // Asynchronously create ReportingQueue.
//   base::ThreadPool::PostTask(
//       FROM_HERE,
//       base::BindOnce(
//           [](google::protobuf::ImportantMessage important_message,
//              reporting::ReportQueue::EnqueueCallback done_cb,
//              std::unique_ptr<reporting::ReportQueueConfiguration> config) {
//             // Asynchronously create ReportingQueue.
//             reporting::ReportQueueProvider::CreateQueue(
//                 std::move(config),
//                 base::BindOnce(
//                     [](std::string_view data,
//                        reporting::ReportQueue::EnqueueCallback
//                        done_cb, reporting::StatusOr<std::unique_ptr<
//                            reporting::ReportQueue>>
//                            report_queue_result) {
//                       // Bail out if queue failed to create.
//                       if (!report_queue_result.has_value()) {
//                         std::move(done_cb).Run(report_queue_result.error());
//                         return;
//                       }
//                       // Queue created successfully, enqueue the message.
//                       report_queue_result.value()->Enqueue(
//                           important_message, std::move(done_cb));
//                     },
//                     important_message, std::move(done_cb)));
//           },
//           important_message, std::move(done_cb),
//           std::move(config_result.value())))
// }
class ReportQueueProvider {
 public:
  // `ReportQueueProvider` and its descendants need to be destructed on
  // sequenced task runners; to facilitate that the following flavor of smart
  // pointer is declared.
  template <typename T>
  using SmartPtr = std::unique_ptr<T, base::OnTaskRunnerDeleter>;

  using CreateReportQueueResponse = StatusOr<std::unique_ptr<ReportQueue>>;

  // The response will come back utilizing the ReportQueueProvider's thread. It
  // is likely that within Chromium you will want to the response to come back
  // on your own thread. The simplest way to achieve that is to pass a
  // base::BindPostTask rather than a base::OnceCallback. Another way to achieve
  // the same result is to utilize base::OnceCallback, and capture the response
  // and forward it to your own thread. We maintain base::OnceCallback here for
  // use in ChromiumOS.
  using CreateReportQueueCallback =
      base::OnceCallback<void(CreateReportQueueResponse)>;

  using OnStorageModuleCreatedCallback =
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>;
  using StorageModuleCreateCallback =
      base::RepeatingCallback<void(OnStorageModuleCreatedCallback)>;

  // Callback triggered with updated report queue config after it has been
  // configured with appropriate DM tokens after it is retrieved. Typically,
  // this is when we go ahead and create the report queue.
  using ReportQueueConfiguredCallback = base::OnceCallback<void(
      StatusOr<std::unique_ptr<ReportQueueConfiguration>>)>;

  ReportQueueProvider(const ReportQueueProvider& other) = delete;
  ReportQueueProvider& operator=(const ReportQueueProvider& other) = delete;
  virtual ~ReportQueueProvider();

  // Asynchronously creates a queue based on the configuration. In the process
  // `ReportQueueProvider` is expected to exist and could still be initializing
  // (if initialization fails, `CreateQueue` will return with error, but next
  // attempt may succeed).
  // Returns with the callback handing ownership to the caller (unless there is
  // an error, and then it gets the error status).
  static void CreateQueue(std::unique_ptr<ReportQueueConfiguration> config,
                          CreateReportQueueCallback queue_cb);

  // Synchronously creates a speculative queue based on the configuration.
  // Returns result as a smart pointer to ReportQueue with the on-thread
  // deleter.
  static StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
  CreateSpeculativeQueue(std::unique_ptr<ReportQueueConfiguration> config);

  // Retrieves current `ReportQueueProvider` instance (created before and
  // referring to `StorageModuleInterface` and optional `Uploader`).
  static ReportQueueProvider* GetInstance();

  static bool IsEncryptedReportingPipelineEnabled();

  // Accessors.
  base::WeakPtr<ReportQueueProvider> GetWeakPtr();
  scoped_refptr<StorageModuleInterface> storage() const;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner() const;

  // Storage module creator (can be substituted for testing purposes).
  StorageModuleCreateCallback storage_create_cb_;

 protected:
  ReportQueueProvider(StorageModuleCreateCallback storage_create_cb,
                      scoped_refptr<base::SequencedTaskRunner> seq_task_runner);

 private:
  // Holds the creation request for a ReportQueue.
  class CreateReportQueueRequest;

  // Finalizes provider, if the initialization process succeeded.
  // May to be overridden by subclass to make more updates to the provider.
  virtual void OnInitCompleted();

  // Creates and initializes queue implementation. Returns status in case of
  // error.
  virtual void CreateNewQueue(std::unique_ptr<ReportQueueConfiguration> config,
                              CreateReportQueueCallback cb);
  virtual StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
  CreateNewSpeculativeQueue(
      const ReportQueue::SpeculativeConfigSettings& config_settings);

  // Configures a given report queue config with appropriate DM tokens after its
  // retrieval so it can be used for downstream processing while building a
  // report queue, and triggers the corresponding completion callback with the
  // updated config.
  virtual void ConfigureReportQueue(
      std::unique_ptr<ReportQueueConfiguration> report_queue_config,
      ReportQueueConfiguredCallback completion_cb) = 0;

  // Checks whether the provider has been initialized, and if so, processes all
  // pending queue creation requests.
  void CheckInitializationState();

  // Processes storage or error returned by async call to `storage_create_cb_`.
  void OnStorageModuleConfigured(
      StatusOr<scoped_refptr<StorageModuleInterface>> storage_result);

  // Task runner used for guarding the provider elements.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Queue for storing creation requests while the provider is
  // initializing.
  std::queue<std::unique_ptr<CreateReportQueueRequest>> create_request_queue_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Storage module associated with the provider. It serves all queues created
  // by it. `storage_` is null initially and when it becomes non-null, the
  // provider is ready to create actual queues (speculative queues can be
  // created before that as well).
  scoped_refptr<StorageModuleInterface> storage_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Weak pointer factory.
  base::WeakPtrFactory<ReportQueueProvider> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_H_

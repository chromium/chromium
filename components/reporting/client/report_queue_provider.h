// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_H_
#define COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/shared_queue.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// ReportQueueProvider acts a single point for instantiating
// |reporting::ReportQueue|s. By performing initialization atomically it ensures
// that all ReportQueues are created with the same global settings.
//
// In order to utilize the ReportQueueProvider the EncryptedReportingPipeline
// feature must be turned on using --enable-features=EncryptedReportingPipeline.
//
// ReportQueueProvider is a singleton which can be accessed through
// |ReportQueueProvider::GetInstance|. This static method must be implemented
// for a specific configuration - Chrome (for ChromeOS and for other OSes),
// other ChromeOS executables.
//
// Example Usage:
// void SendMessage(google::protobuf::ImportantMessage important_message,
//                  reporting::ReportQueue::EnqueueCallback done_cb) {
//   // Create configuration.
//   auto config_result = reporting::ReportQueueConfiguration::Create(...);
//   // Bail out if configuration failed to create.
//   if (!config_result.ok()) {
//     std::move(done_cb).Run(config_result.status());
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
//                     [](base::StringPiece data,
//                        reporting::ReportQueue::EnqueueCallback
//                        done_cb, reporting::StatusOr<std::unique_ptr<
//                            reporting::ReportQueue>>
//                            report_queue_result) {
//                       // Bail out if queue failed to create.
//                       if (!report_queue_result.ok()) {
//                         std::move(done_cb).Run(report_queue_result.status());
//                         return;
//                       }
//                       // Queue created successfully, enqueue the message.
//                       report_queue_result.ValueOrDie()->Enqueue(
//                           important_message, std::move(done_cb));
//                     },
//                     important_message, std::move(done_cb)));
//           },
//           important_message, std::move(done_cb),
//           std::move(config_result.ValueOrDie())))
// }
class ReportQueueProvider {
 public:
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

  using InitCompleteCallback = base::OnceCallback<void(Status)>;

  using OnStorageModuleCreatedCallback =
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>;
  using StorageModuleCreateCallback =
      base::RepeatingCallback<void(OnStorageModuleCreatedCallback)>;

  // Callback triggered with updated report queue config after it has been
  // configured with appropriate DM tokens after it is retrieved. Typically,
  // this is when we go ahead and create the report queue.
  using ReportQueueConfiguredCallback = base::OnceCallback<void(
      StatusOr<std::unique_ptr<ReportQueueConfiguration>>)>;

  explicit ReportQueueProvider(StorageModuleCreateCallback storage_create_cb);
  ReportQueueProvider(const ReportQueueProvider& other) = delete;
  ReportQueueProvider& operator=(const ReportQueueProvider& other) = delete;
  virtual ~ReportQueueProvider();

  // Asynchronously creates a queue based on the configuration. In the process
  // singleton ReportQueueProvider is potentially created and retrieved
  // internally, and then started to initialize (if initialization fails,
  // |CreateQueue| will return with error, but next attempt may succeed).
  // Returns with the callback handing ownership to the caller (unless there is
  // an error, and then it gets the error status).
  static void CreateQueue(std::unique_ptr<ReportQueueConfiguration> config,
                          CreateReportQueueCallback queue_cb);

  // Synchronously creates a speculative queue based on the configuration.
  // Returns result as a smart pointer to ReportQueue with the on-thread
  // deleter.
  static StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
  CreateSpeculativeQueue(std::unique_ptr<ReportQueueConfiguration> config);

  // Instantiates ReportQueueProvider singleton based on the overall process
  // state and will refer to StorageModuleInterface and optional Uploader
  // accordingly.
  static ReportQueueProvider* GetInstance();

  static bool IsEncryptedReportingPipelineEnabled();
  static const base::Feature kEncryptedReportingPipeline;

 protected:
  // Accessors.
  scoped_refptr<StorageModuleInterface> storage();
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner();

  class InitializationStateTracker
      : public base::RefCountedThreadSafe<InitializationStateTracker> {
   public:
    using ReleaseLeaderCallback = base::OnceCallback<void(bool)>;
    using LeaderPromotionRequestCallback =
        base::OnceCallback<void(StatusOr<ReleaseLeaderCallback>)>;
    using GetInitStateCallback = base::OnceCallback<void(bool)>;

    static scoped_refptr<InitializationStateTracker> Create();

    // Will call |get_init_state_cb| with |is_initialized_| value.
    void GetInitState(GetInitStateCallback get_init_state_cb);

    // Will promote one initializer to leader at a time. Will deny
    // initialization requests if the provider is already initialized. If
    // there are no errors will return a ReleaseLeaderCallback for releasing the
    // initializing leadership.
    //
    // Error code responses:
    // RESOURCE_EXHAUSTED - Returned when a promotion is requested when there is
    //     already a leader.
    // FAILED_PRECONDITION - Returned when a promotion is requested when
    //     provider is already initialized.
    void RequestLeaderPromotion(
        LeaderPromotionRequestCallback promo_request_cb);

   private:
    friend class base::RefCountedThreadSafe<InitializationStateTracker>;
    InitializationStateTracker();
    virtual ~InitializationStateTracker();

    void OnIsInitializedRequest(GetInitStateCallback get_init_state_cb);

    void OnLeaderPromotionRequest(
        LeaderPromotionRequestCallback promo_request_cb);

    void ReleaseLeader(bool initialization_successful);
    void OnLeaderRelease(bool initialization_successful);

    bool has_promoted_initializing_context_{false};
    bool is_initialized_{false};

    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
    SEQUENCE_CHECKER(sequence_checker_);
  };

 protected:
  // Storage module creator (can be substituted for testing purposes).
  StorageModuleCreateCallback storage_create_cb_;

 private:
  // Holds the creation request for a ReportQueue.
  class CreateReportQueueRequest {
   public:
    CreateReportQueueRequest(std::unique_ptr<ReportQueueConfiguration> config,
                             CreateReportQueueCallback create_cb);
    ~CreateReportQueueRequest();
    CreateReportQueueRequest(CreateReportQueueRequest&& other);

    std::unique_ptr<ReportQueueConfiguration> config();
    CreateReportQueueCallback create_cb();

   private:
    std::unique_ptr<ReportQueueConfiguration> config_;
    CreateReportQueueCallback create_cb_;
  };
  class InitializingContext;

  // Finalizes provider, if the initialization process succeeded.
  // May to be overridden by subclass to make more updates to the provider.
  virtual void OnInitCompleted();

  // Creates and initializes queue implementation. Returns status in case of
  // error.
  virtual void CreateNewQueue(std::unique_ptr<ReportQueueConfiguration> config,
                              CreateReportQueueCallback cb);
  virtual StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
  CreateNewSpeculativeQueue();

  // Configures a given report queue config with appropriate DM tokens after its
  // retrieval so it can be used for downstream processing while building a
  // report queue, and triggers the corresponding completion callback with the
  // updated config.
  virtual void ConfigureReportQueue(
      std::unique_ptr<ReportQueueConfiguration> report_queue_config,
      ReportQueueConfiguredCallback completion_cb) = 0;

  void OnPushComplete();
  void OnInitState(bool provider_configured);
  void OnInitializationComplete(Status init_status);

  void ClearRequestQueue(base::queue<CreateReportQueueRequest> failed_requests);
  void BuildRequestQueue(StatusOr<CreateReportQueueRequest> pop_result);

  // Queue for storing creation requests while the provider is
  // initializing.
  scoped_refptr<SharedQueue<CreateReportQueueRequest>> create_request_queue_;
  scoped_refptr<InitializationStateTracker> init_state_tracker_;

  // Storage module associated with the provider. It serves all queues created
  // by it. Protected by sequenced_task_runner_.
  scoped_refptr<StorageModuleInterface> storage_;

  // Task runner used for guarding the provider elements.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_H_

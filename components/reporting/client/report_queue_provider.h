// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_H_
#define COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/shared_queue.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

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

  ReportQueueProvider();
  virtual ~ReportQueueProvider();

  ReportQueueProvider(const ReportQueueProvider& other) = delete;
  ReportQueueProvider& operator=(const ReportQueueProvider& other) = delete;

  // Asynchronously creates a queue based on the configuration. In the process
  // singleton ReportQueueProvider is potentially created and retrieved
  // internally, and then started to initialize (if initialization fails,
  // |CreateQueue| will return with error, but next attempt may succeed).
  // Returns with the callback handing ownership to the caller (unless there is
  // an error, and then it gets the error status).
  static void CreateQueue(std::unique_ptr<ReportQueueConfiguration> config,
                          CreateReportQueueCallback queue_cb);

  // Instantiates ReportQueueProvider singleton based on the overall process
  // state and will refer to StorageModuleInterface and optional Uploader
  // accordingly.
  static ReportQueueProvider* GetInstance();

  static bool IsEncryptedReportingPipelineEnabled();
  static const base::Feature kEncryptedReportingPipeline;

 protected:
  scoped_refptr<StorageModuleInterface> storage();

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

  // Context of a single initialization of the ReportQueueProvider.
  // Constructed by |ReportQueueProvider|::|CreateInitializingContext|
  // (for non-trivial initialization needs to be subclassed).
  // Once Start is called, it can post an arbitrary set of callbacks
  // to appropriate threads, and then collects the result back.
  // Once the result is collected, needs to make a call to |Complete|
  // passing resulting status (if status is OK, |OnCompleted| will be
  // called, and it may update the |ReportQueueProvider|).
  // In order to substitute the context, override
  // |ReportQueueProvider::InstantiateInitializingContext|
  //
  // Example:
  //   InitializingContext* InstantiateInitializingContext(
  //       InitCompleteCallback init_complete_cb,
  //       scoped_refptr<InitializationStateTracker> init_state_tracker,
  //       ...more parameters as needed...) override {
  //     return new InitializingContextImpl(std::move(init_complete_cb),
  //                                        init_state_tracker,
  //                                        ...more parameters as needed...,
  //                                        this);
  //   }
  //   class InitializingContextImpl
  //       : public ReportQueueProvider::InitializingContext {
  //    public:
  //     InitializingContextImpl(
  //         InitCompleteCallback init_complete_cb,
  //         scoped_refptr<InitializationStateTracker> init_state_tracker,
  //         ...more parameters as needed...,
  //         ReportQueueProviderImpl* provider)
  //         : InitializingContext(std::move(init_complete_cb),
  //                               init_state_tracker),
  //           ...saving parameters to the context,
  //           provider_(provider) {
  //       DCHECK(provider_ != nullptr);
  //     }
  //
  //    private:
  //     ~InitializingContextImpl() override = default;
  //
  //     void OnStart() override {
  //       ...do work, update state if needed
  //       // Post first callback task
  //       base::ThreadPool::PostTask(
  //           FROM_HERE, {base::TaskPriority::BEST_EFFORT},
  //           base::BindOnce(&InitializingContext::Stage1,
  //           base::Unretained(this),
  //                          Status::StatusOK()));
  //     }
  //
  //     void Stage1() {
  //       ...do work, update state if needed
  //       // Post next callback task
  //       base::ThreadPool::PostTask(
  //           FROM_HERE, {base::TaskPriority::BEST_EFFORT},
  //           base::BindOnce(&InitializingContext::Stage2,
  //           base::Unretained(this),
  //                          Status::StatusOK()));
  //     }
  //
  //     ... more stages as needed
  //
  //     void LastStage() {
  //       ...do the rest of the work, update state if needed
  //       // Post completion task.
  //       base::ThreadPool::PostTask(
  //           FROM_HERE, {base::TaskPriority::BEST_EFFORT},
  //           base::BindOnce(&InitializingContext::Complete,
  //           base::Unretained(this),
  //                          Status::StatusOK()));
  //     }
  //
  //     void OnCompleted() override {
  //       // Hand over all required state to the provider.
  //       provider_->... = std::move(...);
  //     }
  //
  //     ... state
  //     ReportQueueProviderImpl* const provider_;
  //   };
  using InitCompleteCallback = base::OnceCallback<void(Status)>;
  class InitializingContext {
   public:
    InitializingContext(
        InitCompleteCallback init_complete_cb,
        scoped_refptr<InitializationStateTracker> init_state_tracker);

    // Start initialization.
    void Start();

    // Initialization is done, responds with status and self-destructs.
    void Complete(Status status);

   protected:
    // Destructor only called from Complete().
    // The class runs a series of callbacks each of which may invoke
    // either the next callback or Complete(). Thus eventually Complete()
    // is always called and InitializingContext instance is self-destruct.
    virtual ~InitializingContext();

    // Called if |Complete| got a success.
    // Needs to be overridden to update the provider.
    virtual void OnCompleted() = 0;

   private:
    // Called Upon leader promotion: OK means we are a leader and initialization
    // can start by calling OnStart. ALREADY_EXIST means abother leader has been
    // assigned already. Any other code indicates an initialization error.
    void OnLeaderPromotionResult(
        StatusOr<InitializationStateTracker::ReleaseLeaderCallback>
            promo_result);

    // OnStart will begin the process of configuring the ReportQueueProvider.
    // Must be implemented by a subclass, and call Complete to finish
    // initialization.
    virtual void OnStart() = 0;

    InitializationStateTracker::ReleaseLeaderCallback release_leader_cb_;
    scoped_refptr<InitializationStateTracker> init_state_tracker_;

    InitCompleteCallback init_complete_cb_;
  };

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

  // Instantiate InitializationContext subclass for the provider.
  // Result owned by itself (passed to InitializingContext::Start and
  // self-destructs upon InitializingContext::Complete call).
  virtual InitializingContext* InstantiateInitializingContext(
      InitCompleteCallback init_complete_cb,
      scoped_refptr<InitializationStateTracker> init_state_tracker) = 0;

  // Creates and initializes queue implementation. Returns status in case of
  // error.
  virtual CreateReportQueueResponse CreateNewQueue(
      std::unique_ptr<ReportQueueConfiguration> config) = 0;

  void OnPushComplete();
  void OnInitState(bool provider_configured);
  void OnInitializationComplete(Status init_status);

  void ClearRequestQueue(base::queue<CreateReportQueueRequest> failed_requests);
  void BuildRequestQueue(StatusOr<CreateReportQueueRequest> pop_result);

  // Queue for storing creation requests while the provider is
  // initializing.
  scoped_refptr<SharedQueue<CreateReportQueueRequest>> create_request_queue_;
  scoped_refptr<InitializationStateTracker> init_state_tracker_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_H_

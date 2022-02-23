// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_provider.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_impl.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/shared_queue.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// ReportQueueProvider core implementation.

// static
bool ReportQueueProvider::IsEncryptedReportingPipelineEnabled() {
  return base::FeatureList::IsEnabled(kEncryptedReportingPipeline);
}

// static
const base::Feature ReportQueueProvider::kEncryptedReportingPipeline{
    "EncryptedReportingPipeline", base::FEATURE_ENABLED_BY_DEFAULT};

// Context of a single initialization of the ReportQueueProvider.
// Once the result is collected, makes a call to |Complete|
// passing resulting status (if status is OK,
// |ReportQueueProvider::OnInitCompleted| will be called, and it may further
// update the provider).
class ReportQueueProvider::InitializingContext {
 public:
  InitializingContext(
      ReportQueueProvider* provider,
      InitCompleteCallback init_complete_cb,
      scoped_refptr<InitializationStateTracker> init_state_tracker);

  // Start initialization.
  void Start();

  // Initialization is done, responds with status and self-destructs.
  void Complete(Status status);

 protected:
  // Accessor to the owning provider.
  ReportQueueProvider* provider() const;

  // Destructor only called from Complete().
  // The class runs a series of callbacks each of which may invoke
  // either the next callback or Complete(). Thus eventually Complete()
  // is always called and InitializingContext instance is self-destruct.
  ~InitializingContext();

 private:
  // Called upon leader promotion: OK means we are a leader and initialization
  // can start by calling OnStart. ALREADY_EXIST means another leader has been
  // assigned already. Any other code indicates an initialization error.
  void OnLeaderPromotionResult(
      StatusOr<InitializationStateTracker::ReleaseLeaderCallback> promo_result);

  // Handles StorageModule instantiation for the provider to refer to.
  void OnStorageModuleConfigured(
      StatusOr<scoped_refptr<StorageModuleInterface>> storage_result);

  const raw_ptr<ReportQueueProvider> provider_;

  InitializationStateTracker::ReleaseLeaderCallback release_leader_cb_;
  scoped_refptr<InitializationStateTracker> init_state_tracker_;

  InitCompleteCallback init_complete_cb_;
};

ReportQueueProvider::ReportQueueProvider(
    StorageModuleCreateCallback storage_create_cb)
    : storage_create_cb_(storage_create_cb),
      create_request_queue_(SharedQueue<CreateReportQueueRequest>::Create()),
      init_state_tracker_(
          ReportQueueProvider::InitializationStateTracker::Create()),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ReportQueueProvider::~ReportQueueProvider() = default;

void ReportQueueProvider::CreateNewQueue(
    std::unique_ptr<ReportQueueConfiguration> config,
    CreateReportQueueCallback cb) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ReportQueueProvider* provider,
             std::unique_ptr<ReportQueueConfiguration> config,
             CreateReportQueueCallback cb) {
            // Configure report queue config with an appropriate DM token and
            // proceed to create the queue if configuration was successful
            auto report_queue_configured_cb = base::BindOnce(
                [](scoped_refptr<StorageModuleInterface> storage,
                   CreateReportQueueCallback cb,
                   StatusOr<std::unique_ptr<ReportQueueConfiguration>>
                       config_result) {
                  // If configuration hit an error, we abort and
                  // report this through the callback
                  if (!config_result.ok()) {
                    std::move(cb).Run(config_result.status());
                    return;
                  }

                  // proceed to create the queue
                  ReportQueueImpl::Create(std::move(config_result.ValueOrDie()),
                                          storage, std::move(cb));
                },
                provider->storage(), std::move(cb));

            provider->ConfigureReportQueue(
                std::move(config), std::move(report_queue_configured_cb));
          },
          this, std::move(config), std::move(cb)));
}

StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
ReportQueueProvider::CreateNewSpeculativeQueue() {
  return SpeculativeReportQueueImpl::Create();
}

void ReportQueueProvider::OnInitCompleted() {}

// static
void ReportQueueProvider::CreateQueue(
    std::unique_ptr<ReportQueueConfiguration> config,
    CreateReportQueueCallback create_cb) {
  if (!IsEncryptedReportingPipelineEnabled()) {
    Status not_enabled = Status(
        error::FAILED_PRECONDITION,
        "The Encrypted Reporting Pipeline is not enabled. Please enable it on "
        "the command line using --enable-features=EncryptedReportingPipeline");
    VLOG(1) << not_enabled;
    std::move(create_cb).Run(not_enabled);
    return;
  }
  auto* instance = GetInstance();
  instance->create_request_queue_->Push(
      CreateReportQueueRequest(std::move(config), std::move(create_cb)),
      base::BindOnce(&ReportQueueProvider::OnPushComplete,
                     base::Unretained(instance)));
}

// static
StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
ReportQueueProvider::CreateSpeculativeQueue(
    std::unique_ptr<ReportQueueConfiguration> config) {
  if (!IsEncryptedReportingPipelineEnabled()) {
    Status not_enabled = Status(
        error::FAILED_PRECONDITION,
        "The Encrypted Reporting Pipeline is not enabled. Please enable it on "
        "the command line using --enable-features=EncryptedReportingPipeline");
    VLOG(1) << not_enabled;
    return not_enabled;
  }
  auto* instance = GetInstance();
  // Instantiate speculative queue.
  auto speculative_queue_result = instance->CreateNewSpeculativeQueue();
  if (!speculative_queue_result.ok()) {
    return speculative_queue_result.status();
  }
  // Initiate underlying queue creation.
  auto speculative_queue = std::move(speculative_queue_result.ValueOrDie());
  instance->create_request_queue_->Push(
      CreateReportQueueRequest(std::move(config),
                               speculative_queue->PrepareToAttachActualQueue()),
      base::BindOnce(&ReportQueueProvider::OnPushComplete,
                     base::Unretained(instance)));
  return speculative_queue;
}

void ReportQueueProvider::OnPushComplete() {
  init_state_tracker_->GetInitState(base::BindOnce(
      &ReportQueueProvider::OnInitState, base::Unretained(this)));
}

void ReportQueueProvider::OnInitState(bool provider_configured) {
  if (!provider_configured) {
    // Schedule an InitializingContext to take care of initialization.
    InitializingContext* const context = new InitializingContext(
        this,
        base::BindOnce(&ReportQueueProvider::OnInitializationComplete,
                       base::Unretained(this)),
        init_state_tracker_);
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(&InitializingContext::Start, base::Unretained(context)));
    return;
  }

  // Client was configured, build the queue!
  create_request_queue_->Pop(base::BindOnce(
      &ReportQueueProvider::BuildRequestQueue, base::Unretained(this)));
}

void ReportQueueProvider::OnInitializationComplete(Status init_status) {
  if (init_status.error_code() == error::RESOURCE_EXHAUSTED) {
    // This happens when a new request comes in while the ReportQueueProvider is
    // undergoing initialization. The leader will either clear or build the
    // queue when it completes.
    return;
  }

  // Configuration failed. Clear out all the requests that came in while we were
  // attempting to configure.
  if (!init_status.ok()) {
    create_request_queue_->Swap(
        base::queue<CreateReportQueueRequest>(),
        base::BindOnce(&ReportQueueProvider::ClearRequestQueue,
                       base::Unretained(this)));
    return;
  }
  create_request_queue_->Pop(base::BindOnce(
      &ReportQueueProvider::BuildRequestQueue, base::Unretained(this)));
}

void ReportQueueProvider::ClearRequestQueue(
    base::queue<CreateReportQueueRequest> failed_requests) {
  while (!failed_requests.empty()) {
    // Post to general thread.
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(
                       [](CreateReportQueueRequest queue_request) {
                         std::move(queue_request.create_cb())
                             .Run(Status(error::UNAVAILABLE,
                                         "Unable to build a ReportQueue"));
                       },
                       std::move(failed_requests.front())));
    failed_requests.pop();
  }
}

void ReportQueueProvider::BuildRequestQueue(
    StatusOr<CreateReportQueueRequest> pop_result) {
  // Queue is clear - nothing more to do.
  if (!pop_result.ok()) {
    return;
  }

  // We don't want to block either the ReportQueueProvider
  // sequenced_task_runner_ or the create_request_queue_.sequenced_task_runner_,
  // so we post the task to a general thread.
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ReportQueueProvider* provider,
             CreateReportQueueRequest report_queue_request) {
            provider->CreateNewQueue(report_queue_request.config(),
                                     report_queue_request.create_cb());
          },
          base::Unretained(this), std::move(pop_result.ValueOrDie())));

  // Build the next item asynchronously
  create_request_queue_->Pop(base::BindOnce(
      &ReportQueueProvider::BuildRequestQueue, base::Unretained(this)));
}

// CreateReportQueueRequest implementation.

ReportQueueProvider::CreateReportQueueRequest::CreateReportQueueRequest(
    std::unique_ptr<ReportQueueConfiguration> config,
    CreateReportQueueCallback create_cb)
    : config_(std::move(config)), create_cb_(std::move(create_cb)) {}

ReportQueueProvider::CreateReportQueueRequest::~CreateReportQueueRequest() =
    default;

ReportQueueProvider::CreateReportQueueRequest::CreateReportQueueRequest(
    ReportQueueProvider::CreateReportQueueRequest&& other)
    : config_(other.config()), create_cb_(other.create_cb()) {}

std::unique_ptr<ReportQueueConfiguration>
ReportQueueProvider::CreateReportQueueRequest::config() {
  return std::move(config_);
}

ReportQueueProvider::CreateReportQueueCallback
ReportQueueProvider::CreateReportQueueRequest::create_cb() {
  return std::move(create_cb_);
}

scoped_refptr<StorageModuleInterface> ReportQueueProvider::storage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_;
}

scoped_refptr<base::SequencedTaskRunner>
ReportQueueProvider::sequenced_task_runner() {
  return sequenced_task_runner_;
}

// InitializingContext implementation.

ReportQueueProvider::InitializingContext::InitializingContext(
    ReportQueueProvider* provider,
    InitCompleteCallback init_complete_cb,
    scoped_refptr<ReportQueueProvider::InitializationStateTracker>
        init_state_tracker)
    : provider_(provider),
      init_state_tracker_(init_state_tracker),
      init_complete_cb_(std::move(init_complete_cb)) {
  DCHECK(provider_);
}

ReportQueueProvider::InitializingContext::~InitializingContext() = default;

ReportQueueProvider* ReportQueueProvider::InitializingContext::provider()
    const {
  return provider_;
}

void ReportQueueProvider::InitializingContext::Start() {
  init_state_tracker_->RequestLeaderPromotion(base::BindOnce(
      &InitializingContext::OnLeaderPromotionResult, base::Unretained(this)));
}

void ReportQueueProvider::InitializingContext::OnLeaderPromotionResult(
    StatusOr<
        ReportQueueProvider::InitializationStateTracker::ReleaseLeaderCallback>
        promo_result) {
  if (!promo_result.ok()) {
    Complete(promo_result.status());
    return;
  }

  release_leader_cb_ = std::move(promo_result.ValueOrDie());

  // Proceed with storage creation asynchronously.
  base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>
      result_cb = base::BindOnce(
          &ReportQueueProvider::InitializingContext::OnStorageModuleConfigured,
          base::Unretained(this));
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](StorageModuleCreateCallback create_cb,
             OnStorageModuleCreatedCallback storage_result_cb) {
            create_cb.Run(std::move(storage_result_cb));
          },
          provider_->storage_create_cb_,
          base::BindPostTask(provider_->sequenced_task_runner_,
                             std::move(result_cb))));
}

void ReportQueueProvider::InitializingContext::OnStorageModuleConfigured(
    StatusOr<scoped_refptr<StorageModuleInterface>> storage_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(provider_->sequence_checker_);
  if (!storage_result.ok()) {
    Complete(storage_result.status());
    return;
  }
  DCHECK(!provider_->storage_) << "Storage module already recorded";
  provider_->storage_ = storage_result.ValueOrDie();
  Complete(Status::StatusOK());
}

void ReportQueueProvider::InitializingContext::Complete(Status status) {
  if (status.error_code() == error::RESOURCE_EXHAUSTED) {
    // There is already a leader initializing the ReportQueueProvider.
    std::move(init_complete_cb_).Run(status);
    delete this;
    return;
  }

  if (status.ok()) {
    // Let provider subclass do final adjustments in case of success.
    provider_->OnInitCompleted();
    std::move(release_leader_cb_).Run(/*initialization_successful=*/true);
  } else if (status.error_code() == error::ALREADY_EXISTS) {
    // Between building this InitializingContext and attempting to promote to
    // leader, the |ReportQueueProvider| was configured. Respond Ok but do not
    // update the provider.
    status = Status::StatusOK();
  }

  std::move(init_complete_cb_).Run(status);
  delete this;
}

// InitializationStateTracker implementation.

ReportQueueProvider::InitializationStateTracker::InitializationStateTracker()
    : sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ReportQueueProvider::InitializationStateTracker::~InitializationStateTracker() =
    default;

// static
scoped_refptr<ReportQueueProvider::InitializationStateTracker>
ReportQueueProvider::InitializationStateTracker::Create() {
  return base::WrapRefCounted(
      new ReportQueueProvider::InitializationStateTracker());
}

void ReportQueueProvider::InitializationStateTracker::GetInitState(
    GetInitStateCallback get_init_state_cb) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ReportQueueProvider::InitializationStateTracker::
                         OnIsInitializedRequest,
                     this, std::move(get_init_state_cb)));
}

void ReportQueueProvider::InitializationStateTracker::RequestLeaderPromotion(
    LeaderPromotionRequestCallback promo_request_cb) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ReportQueueProvider::InitializationStateTracker::
                         OnLeaderPromotionRequest,
                     this, std::move(promo_request_cb)));
}

void ReportQueueProvider::InitializationStateTracker::OnIsInitializedRequest(
    GetInitStateCallback get_init_state_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](GetInitStateCallback get_init_state_cb, bool is_initialized) {
            std::move(get_init_state_cb).Run(is_initialized);
          },
          std::move(get_init_state_cb), is_initialized_));
}

void ReportQueueProvider::InitializationStateTracker::OnLeaderPromotionRequest(
    LeaderPromotionRequestCallback promo_request_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StatusOr<ReleaseLeaderCallback> result;
  if (is_initialized_) {
    result = Status(error::ALREADY_EXISTS,
                    "ReportQueueProvider is already configured");
  } else if (has_promoted_initializing_context_) {
    result =
        Status(error::RESOURCE_EXHAUSTED,
               "ReportQueueProvider already has a lead initializing context.");
  } else {
    has_promoted_initializing_context_ = true;
    result = base::BindOnce(
        &ReportQueueProvider::InitializationStateTracker::ReleaseLeader, this);
  }

  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(
                     [](LeaderPromotionRequestCallback promo_request_cb,
                        StatusOr<ReleaseLeaderCallback> result) {
                       std::move(promo_request_cb).Run(std::move(result));
                     },
                     std::move(promo_request_cb), std::move(result)));
}

void ReportQueueProvider::InitializationStateTracker::ReleaseLeader(
    bool initialization_successful) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReportQueueProvider::InitializationStateTracker::OnLeaderRelease,
          this, initialization_successful));
}

void ReportQueueProvider::InitializationStateTracker::OnLeaderRelease(
    bool initialization_successful) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialization_successful) {
    is_initialized_ = true;
  }
  has_promoted_initializing_context_ = false;
}

}  // namespace reporting

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_provider.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/shared_queue.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace reporting {

// ReportQueueProvider core implementation.

// static
bool ReportQueueProvider::IsEncryptedReportingPipelineEnabled() {
  return base::FeatureList::IsEnabled(kEncryptedReportingPipeline);
}

// static
const base::Feature ReportQueueProvider::kEncryptedReportingPipeline{
    "EncryptedReportingPipeline", base::FEATURE_DISABLED_BY_DEFAULT};

ReportQueueProvider::ReportQueueProvider()
    : create_request_queue_(SharedQueue<CreateReportQueueRequest>::Create()),
      init_state_tracker_(
          ReportQueueProvider::InitializationStateTracker::Create()) {}

ReportQueueProvider::~ReportQueueProvider() = default;

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

void ReportQueueProvider::OnPushComplete() {
  init_state_tracker_->GetInitState(base::BindOnce(
      &ReportQueueProvider::OnInitState, base::Unretained(this)));
}

void ReportQueueProvider::OnInitState(bool provider_configured) {
  if (!provider_configured) {
    // Schedule an InitializingContext to take care of initialization.
    InitializingContext* const context = InstantiateInitializingContext(
        base::BindOnce(&ReportQueueProvider::OnConfigResult,
                       base::Unretained(this)),
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

void ReportQueueProvider::OnConfigResult(
    base::OnceCallback<void(Status)> continue_init_cb) {
  std::move(continue_init_cb).Run(Status::StatusOK());
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
            std::move(report_queue_request.create_cb())
                .Run(provider->CreateNewQueue(report_queue_request.config()));
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

// InitializingContext implementation.

ReportQueueProvider::InitializingContext::InitializingContext(
    UpdateConfigurationCallback update_config_cb,
    InitCompleteCallback init_complete_cb,
    scoped_refptr<ReportQueueProvider::InitializationStateTracker>
        init_state_tracker)
    : update_config_cb_(std::move(update_config_cb)),
      init_state_tracker_(init_state_tracker),
      init_complete_cb_(std::move(init_complete_cb)) {}

ReportQueueProvider::InitializingContext::~InitializingContext() = default;

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

  // Proceed with configuration asynchronously.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReportQueueProvider::InitializingContext::OnStart,
                     base::Unretained(this)));
}

void ReportQueueProvider::InitializingContext::OnCompleted() {
  std::move(update_config_cb_)
      .Run(base::BindOnce(&InitializingContext::Complete,
                          base::Unretained(this)));
}

void ReportQueueProvider::InitializingContext::Complete(Status status) {
  if (status.ok()) {
    // Export the results to the provider.
    OnCompleted();
  } else if (status.error_code() == error::ALREADY_EXISTS) {
    // Between building this InitializingContext and attempting to promote to
    // leader, the |ReportQueueProvider| was configured. Respond Ok but do not
    // update the provider.
    status = Status::StatusOK();
  }
  std::move(release_leader_cb_).Run(/*initialization_successful=*/status.ok());
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
    result =
        Status(error::ALREADY_EXISTS, "ReportClient is already configured");
  } else if (has_promoted_initializing_context_) {
    result = Status(error::RESOURCE_EXHAUSTED,
                    "ReportClient already has a lead initializing context.");
  } else {
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
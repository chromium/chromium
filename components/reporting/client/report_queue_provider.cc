// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_provider.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
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
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

using InitCompleteCallback = base::OnceCallback<void(Status)>;

// Report queue creation request. Recorded in the `create_request_queue_` when
// provider cannot create queues yet.
class ReportQueueProvider::CreateReportQueueRequest {
 public:
  static void New(std::unique_ptr<ReportQueueConfiguration> config,
                  CreateReportQueueCallback create_cb) {
    auto* const provider = GetInstance();
    DCHECK(provider)
        << "Provider must exist, otherwise it is an internal error";
    auto request = base::WrapUnique(
        new CreateReportQueueRequest(std::move(config), std::move(create_cb)));
    provider->sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<ReportQueueProvider> provider,
               std::unique_ptr<CreateReportQueueRequest> request) {
              if (!provider) {
                std::move(request->release_create_cb())
                    .Run(Status(error::UNAVAILABLE,
                                "Provider has been shut down"));
                return;
              }
              provider->state_->PushRequest(std::move(request));
              provider->CheckInitializationState();
            },
            provider->state_->GetWeakPtr(), std::move(request)));
  }

  CreateReportQueueRequest(const CreateReportQueueRequest& other) = delete;
  CreateReportQueueRequest& operator=(const CreateReportQueueRequest& other) =
      delete;
  ~CreateReportQueueRequest() = default;

  std::unique_ptr<ReportQueueConfiguration> release_config() {
    DCHECK(config_) << "Can only be released once";
    return std::move(config_);
  }

  ReportQueueProvider::CreateReportQueueCallback release_create_cb() {
    DCHECK(create_cb_) << "Can only be released once";
    return std::move(create_cb_);
  }

 private:
  // Constructor is only called by `New` factory method above.
  CreateReportQueueRequest(std::unique_ptr<ReportQueueConfiguration> config,
                           CreateReportQueueCallback create_cb)
      : config_(std::move(config)), create_cb_(std::move(create_cb)) {}

  std::unique_ptr<ReportQueueConfiguration> config_;
  CreateReportQueueCallback create_cb_;
};

// Provider creation state implementation.

ReportQueueProvider::ProviderState::ProviderState(ReportQueueProvider* provider)
    : weak_ptr_factory_(provider) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ReportQueueProvider::ProviderState::~ProviderState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!create_request_queue_.empty()) {
    auto& report_queue_request = create_request_queue_.front();
    std::move(report_queue_request->release_create_cb())
        .Run(Status(error::UNAVAILABLE, "Unable to build a ReportQueue"));
    create_request_queue_.pop();
  }
}

base::WeakPtr<ReportQueueProvider>
ReportQueueProvider::ProviderState::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

size_t ReportQueueProvider::ProviderState::CountRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return create_request_queue_.size();
}

void ReportQueueProvider::ProviderState::PushRequest(
    std::unique_ptr<ReportQueueProvider::CreateReportQueueRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  create_request_queue_.push(std::move(request));
}

std::unique_ptr<ReportQueueProvider::CreateReportQueueRequest>
ReportQueueProvider::ProviderState::PopRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (create_request_queue_.empty()) {
    return nullptr;
  }
  auto request = std::move(create_request_queue_.front());
  create_request_queue_.pop();
  return request;
}

scoped_refptr<StorageModuleInterface>
ReportQueueProvider::ProviderState::storage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_;
}

void ReportQueueProvider::ProviderState::set_storage(
    scoped_refptr<StorageModuleInterface> storage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  storage_ = storage;
}

// ReportQueueProvider core implementation.

// static
bool ReportQueueProvider::IsEncryptedReportingPipelineEnabled() {
  return base::FeatureList::IsEnabled(kEncryptedReportingPipeline);
}

// static
BASE_FEATURE(kEncryptedReportingPipeline,
             "EncryptedReportingPipeline",
             base::FEATURE_ENABLED_BY_DEFAULT);

ReportQueueProvider::ReportQueueProvider(
    StorageModuleCreateCallback storage_create_cb)
    : storage_create_cb_(storage_create_cb),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})),
      state_(new ProviderState(this),
             base::OnTaskRunnerDeleter(sequenced_task_runner_)) {}

ReportQueueProvider::~ReportQueueProvider() = default;

scoped_refptr<StorageModuleInterface> ReportQueueProvider::storage() const {
  return state_->storage();
}

scoped_refptr<base::SequencedTaskRunner>
ReportQueueProvider::sequenced_task_runner() const {
  return sequenced_task_runner_;
}

void ReportQueueProvider::CreateNewQueue(
    std::unique_ptr<ReportQueueConfiguration> config,
    CreateReportQueueCallback cb) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<ReportQueueProvider> provider,
             std::unique_ptr<ReportQueueConfiguration> config,
             CreateReportQueueCallback cb) {
            if (!provider) {
              std::move(cb).Run(
                  Status(error::UNAVAILABLE, "Provider has been shut down"));
              return;
            }
            // Configure report queue config with an appropriate DM token and
            // proceed to create the queue if configuration was successful.
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

                  // Proceed to create the queue on arbitrary thread.
                  base::ThreadPool::PostTask(
                      FROM_HERE,
                      base::BindOnce(&ReportQueueImpl::Create,
                                     std::move(config_result.ValueOrDie()),
                                     storage, std::move(cb)));
                },
                provider->state_->storage(), std::move(cb));

            provider->ConfigureReportQueue(
                std::move(config), std::move(report_queue_configured_cb));
          },
          state_->GetWeakPtr(), std::move(config), std::move(cb)));
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
  CreateReportQueueRequest::New(std::move(config), std::move(create_cb));
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
  // Instantiate speculative queue, bail out in case of an error.
  ASSIGN_OR_RETURN(auto speculative_queue,
                   GetInstance()->CreateNewSpeculativeQueue());
  // Initiate underlying queue creation.
  CreateReportQueueRequest::New(
      std::move(config), speculative_queue->PrepareToAttachActualQueue());
  return speculative_queue;
}

void ReportQueueProvider::CheckInitializationState() {
  if (!state_->storage()) {
    // Provider not ready.
    const auto count = state_->CountRequests();
    DCHECK_GT(count, 0u) << "Request queue cannot be empty";
    if (count > 1u) {
      // More than one request in the queue - it means Storage creation has
      // already been started.
      return;
    }
    // Start Storage creation on an arbitrary thread. Upon completion resume on
    // sequenced task runner.
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(
            [](StorageModuleCreateCallback storage_create_cb,
               OnStorageModuleCreatedCallback on_storage_created_cb) {
              storage_create_cb.Run(std::move(on_storage_created_cb));
            },
            storage_create_cb_,
            base::BindPostTask(
                sequenced_task_runner_,
                base::BindOnce(&ReportQueueProvider::OnStorageModuleConfigured,
                               state_->GetWeakPtr()))));
    return;
  }

  // Storage ready, create all report queues that were submitted.
  // Note that `CreateNewQueue` call offsets heavy work to arbitrary threads.
  while (auto report_queue_request = state_->PopRequest()) {
    CreateNewQueue(report_queue_request->release_config(),
                   report_queue_request->release_create_cb());
  }
}

void ReportQueueProvider::OnStorageModuleConfigured(
    StatusOr<scoped_refptr<StorageModuleInterface>> storage_result) {
  if (!storage_result.ok()) {
    // Storage creation failed, kill all requests.
    while (auto report_queue_request = state_->PopRequest()) {
      std::move(report_queue_request->release_create_cb())
          .Run(Status(error::UNAVAILABLE, "Unable to build a ReportQueue"));
    }
    return;
  }

  // Storage ready, create all report queues that were submitted.
  // Note that `CreateNewQueue` call offsets heavy work to arbitrary threads.
  DCHECK(!state_->storage()) << "Storage module already recorded";
  OnInitCompleted();
  state_->set_storage(storage_result.ValueOrDie());
  while (auto report_queue_request = state_->PopRequest()) {
    CreateNewQueue(report_queue_request->release_config(),
                   report_queue_request->release_create_cb());
  }
}
}  // namespace reporting

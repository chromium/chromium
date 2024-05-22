// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_provider.h"

#include <memory>
#include <string>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_impl.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/reporting_errors.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

using InitCompleteCallback = base::OnceCallback<void(Status)>;

ReportQueueProvider* g_report_queue_provider_instance = nullptr;

// Report queue creation request. Recorded in the `create_request_queue_` when
// provider cannot create queues yet.
class ReportQueueProvider::CreateReportQueueRequest {
 public:
  static void New(std::unique_ptr<ReportQueueConfiguration> config,
                  CreateReportQueueCallback create_cb) {
    auto* const provider = GetInstance();
    CHECK(provider) << "Provider must exist, otherwise it is an internal error";
    auto request = base::WrapUnique(
        new CreateReportQueueRequest(std::move(config), std::move(create_cb)));
    provider->sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<ReportQueueProvider> provider,
               std::unique_ptr<CreateReportQueueRequest> request) {
              if (!provider) {
                std::move(request->release_create_cb())
                    .Run(base::unexpected(Status(
                        error::UNAVAILABLE, "Provider has been shut down")));
                return;
              }
              DCHECK_CALLED_ON_VALID_SEQUENCE(provider->sequence_checker_);
              provider->create_request_queue_.push(std::move(request));
              provider->CheckInitializationState();
            },
            provider->GetWeakPtr(), std::move(request)));
  }

  CreateReportQueueRequest(const CreateReportQueueRequest& other) = delete;
  CreateReportQueueRequest& operator=(const CreateReportQueueRequest& other) =
      delete;
  ~CreateReportQueueRequest() = default;

  std::unique_ptr<ReportQueueConfiguration> release_config() {
    CHECK(config_) << "Can only be released once";
    return std::move(config_);
  }

  ReportQueueProvider::CreateReportQueueCallback release_create_cb() {
    CHECK(create_cb_) << "Can only be released once";
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
    StorageModuleCreateCallback storage_create_cb,
    scoped_refptr<base::SequencedTaskRunner> seq_task_runner)
    : storage_create_cb_(storage_create_cb),
      sequenced_task_runner_(seq_task_runner) {
  if (g_report_queue_provider_instance) {
    CHECK_IS_TEST();  // Duplicate is allowed in tests only.
  }
  g_report_queue_provider_instance = this;
}

ReportQueueProvider::~ReportQueueProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Kill all remaining requests.
  while (!create_request_queue_.empty()) {
    auto& report_queue_request = create_request_queue_.front();
    std::move(report_queue_request->release_create_cb())
        .Run(base::unexpected(
            Status(error::UNAVAILABLE, "ReportQueueProvider shut down")));
    create_request_queue_.pop();
  }
  g_report_queue_provider_instance = nullptr;
}

base::WeakPtr<ReportQueueProvider> ReportQueueProvider::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

scoped_refptr<StorageModuleInterface> ReportQueueProvider::storage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_;
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
              std::move(cb).Run(base::unexpected(
                  Status(error::UNAVAILABLE, "Provider has been shut down")));
              base::UmaHistogramEnumeration(
                  reporting::kUmaUnavailableErrorReason,
                  UnavailableErrorReason::REPORT_QUEUE_PROVIDER_DESTRUCTED,
                  UnavailableErrorReason::MAX_VALUE);
              return;
            }
            // Configure report queue config with an appropriate DM token and
            // proceed to create the queue if configuration was successful.
            DCHECK_CALLED_ON_VALID_SEQUENCE(provider->sequence_checker_);
            auto report_queue_configured_cb = base::BindOnce(
                [](scoped_refptr<StorageModuleInterface> storage,
                   CreateReportQueueCallback cb,
                   StatusOr<std::unique_ptr<ReportQueueConfiguration>>
                       config_result) {
                  // If configuration hit an error, we abort and
                  // report this through the callback
                  if (!config_result.has_value()) {
                    std::move(cb).Run(
                        base::unexpected(std::move(config_result).error()));
                    return;
                  }

                  // Proceed to create the queue on arbitrary thread.
                  base::ThreadPool::PostTask(
                      FROM_HERE,
                      base::BindOnce(&ReportQueueImpl::Create,
                                     std::move(config_result.value()), storage,
                                     std::move(cb)));
                },
                provider->storage_, std::move(cb));

            provider->ConfigureReportQueue(
                std::move(config), std::move(report_queue_configured_cb));
          },
          GetWeakPtr(), std::move(config), std::move(cb)));
}

StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
ReportQueueProvider::CreateNewSpeculativeQueue(
    const ReportQueue::SpeculativeConfigSettings& config_settings) {
  return SpeculativeReportQueueImpl::Create(config_settings);
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
    std::move(create_cb).Run(base::unexpected(not_enabled));
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
    return base::unexpected(std::move(not_enabled));
  }
  // Instantiate speculative queue, bail out in case of an error.
  CHECK(config);
  ASSIGN_OR_RETURN(auto speculative_queue,
                   GetInstance()->CreateNewSpeculativeQueue(
                       {.destination = config->destination()}));
  // Initiate underlying queue creation.
  CreateReportQueueRequest::New(
      std::move(config), speculative_queue->PrepareToAttachActualQueue());
  return speculative_queue;
}

void ReportQueueProvider::CheckInitializationState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!storage_) {
    // Provider not ready.
    CHECK(!create_request_queue_.empty()) << "Request queue cannot be empty";
    if (create_request_queue_.size() > 1) {
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
                               GetWeakPtr()))));
    return;
  }

  // Storage ready, create all report queues that were submitted.
  // Note that `CreateNewQueue` call offsets heavy work to arbitrary threads.
  while (!create_request_queue_.empty()) {
    auto& report_queue_request = create_request_queue_.front();
    CreateNewQueue(report_queue_request->release_config(),
                   report_queue_request->release_create_cb());
    create_request_queue_.pop();
  }
}

void ReportQueueProvider::OnStorageModuleConfigured(
    StatusOr<scoped_refptr<StorageModuleInterface>> storage_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!storage_result.has_value()) {
    // Storage creation failed, kill all requests.
    while (!create_request_queue_.empty()) {
      auto& report_queue_request = create_request_queue_.front();
      std::move(report_queue_request->release_create_cb())
          .Run(base::unexpected(
              Status(error::UNAVAILABLE, "Unable to build a ReportQueue")));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::UNABLE_TO_BUILD_REPORT_QUEUE,
          UnavailableErrorReason::MAX_VALUE);
      create_request_queue_.pop();
    }
    return;
  }

  // Storage ready, create all report queues that were submitted.
  // Note that `CreateNewQueue` call offsets heavy work to arbitrary threads.
  CHECK(!storage_) << "Storage module already recorded";
  OnInitCompleted();
  storage_ = storage_result.value();
  while (!create_request_queue_.empty()) {
    auto& report_queue_request = create_request_queue_.front();
    CreateNewQueue(report_queue_request->release_config(),
                   report_queue_request->release_create_cb());
    create_request_queue_.pop();
  }
}

// static
ReportQueueProvider* ReportQueueProvider::GetInstance() {
  CHECK(g_report_queue_provider_instance)
      << "Report queue provider not set yet";
  return g_report_queue_provider_instance;
}
}  // namespace reporting

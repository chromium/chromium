// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage.h"

#include <string_view>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/key_delivery.h"
#include "components/reporting/storage/storage_base.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_queue.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/storage/storage_util.h"
#include "components/reporting/util/file.h"
#include "components/reporting/util/reporting_errors.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"

namespace reporting {

// Context for creating a single queue. Upon success, calls the callback with
// the GenerationGuid passed into the context, otherwise error status.
class CreateQueueContext : public TaskRunnerContext<Status> {
 public:
  CreateQueueContext(
      Priority priority,
      QueueOptions queue_options,
      scoped_refptr<Storage> storage,
      GenerationGuid generation_guid,
      base::OnceCallback<void(scoped_refptr<reporting::StorageQueue>,
                              base::OnceCallback<void(reporting::Status)>)>
          queue_created_cb,
      base::OnceCallback<void(Status)> completion_cb)
      : TaskRunnerContext<Status>(
            std::move(completion_cb),
            storage->sequenced_task_runner_),  // Same runner as the Storage!
        queue_options_(queue_options),
        storage_(storage),
        generation_guid_(generation_guid),
        priority_(priority),
        queue_created_cb_(std::move(queue_created_cb)) {}

  CreateQueueContext(const CreateQueueContext&) = delete;
  CreateQueueContext& operator=(const CreateQueueContext&) = delete;

 private:
  void OnStart() override {
    CheckOnValidSequence();
    DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);

    // Set the extension of the queue directory name
    queue_options_.set_subdirectory_extension(generation_guid_);

    // Construct the queue
    InitQueue(priority_, queue_options_);
  }

  void InitQueue(Priority priority, QueueOptions queue_options) {
    CheckOnValidSequence();
    // Instantiate queue.
    storage_queue_ = StorageQueue::Create(
        generation_guid_, queue_options,
        // Note: the callback below belongs to the Queue and does not
        // outlive Storage, so it cannot refer to `storage_` itself!
        base::BindRepeating(&QueueUploaderInterface::AsyncProvideUploader,
                            priority, storage_->async_start_upload_cb_,
                            storage_->encryption_module_),
        // `queues_container_` refers a weak pointer only, so that its
        // callback does not hold a reference to it.
        base::BindPostTask(
            storage_->sequenced_task_runner_,
            base::BindRepeating(&QueuesContainer::GetDegradationCandidates,
                                storage_->queues_container_->GetWeakPtr(),
                                priority)),
        base::BindPostTask(
            storage_->sequenced_task_runner_,
            base::BindRepeating(&QueuesContainer::DisableQueue,
                                storage_->queues_container_->GetWeakPtr(),
                                priority)),
        base::BindPostTask(
            storage_->sequenced_task_runner_,
            base::BindRepeating(&QueuesContainer::DisconnectQueue,
                                storage_->queues_container_->GetWeakPtr(),
                                priority)),
        storage_->encryption_module_, storage_->compression_module_);
    // Add queue to the container.
    const auto added_status =
        storage_->queues_container_->AddQueue(priority, storage_queue_);
    if (added_status.ok()) {
      // The queue has been added. Once it is initialized, we will resume at
      // `Initialized` and invoke the `queue_created_cb_` (if successful).
      // Asynchronously run initialization.
      storage_queue_->Init(
          /*init_retry_cb=*/base::BindRepeating(
              &StorageQueue::MaybeBackoffAndReInit),
          /*initialized_cb=*/base::BindPostTaskToCurrentDefault(base::BindOnce(
              &CreateQueueContext::Initialized, base::Unretained(this),
              /*priority=*/priority)));
      return;
    }

    // The queue failed to add. It could happen because the same priority and
    // guid were being added in parallel (could only happen when new
    // multi-generation queues are needed for `Write` operation).
    // We will check whether this is the case, and return that queue instead.
    const auto query_result =
        storage_->queues_container_->GetQueue(priority, generation_guid_);
    if (!query_result.has_value()) {
      // No pre-recorded queue either.
      Response(added_status);
      return;
    }
    // Substitute and use prior queue from now on.
    storage_queue_ = query_result.value();
    // Schedule `Initialized` to be invoked when initialization is done (or
    // immediately, if the queue is already initialized).
    storage_queue_->OnInit(base::BindPostTaskToCurrentDefault(base::BindOnce(
        &CreateQueueContext::Initialized, base::Unretained(this), priority)));
  }

  void Initialized(Priority priority, Status initialization_result) {
    CheckOnValidSequence();
    DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);
    if (!initialization_result.ok()) {
      LOG(ERROR) << "Could not initialize queue for generation_guid="
                 << generation_guid_ << " priority=" << priority
                 << ", error=" << initialization_result;
      Response(initialization_result);
      return;
    }

    // Report success.
    std::move(queue_created_cb_)
        .Run(storage_queue_,
             base::BindPostTaskToCurrentDefault(base::BindOnce(
                 &CreateQueueContext::Response, base::Unretained(this))));
  }

  QueueOptions queue_options_;
  scoped_refptr<StorageQueue> storage_queue_;

  const scoped_refptr<Storage> storage_;
  const GenerationGuid generation_guid_;
  const Priority priority_;
  base::OnceCallback<void(scoped_refptr<reporting::StorageQueue>,
                          base::OnceCallback<void(reporting::Status)>)>
      queue_created_cb_;
};

void Storage::Create(
    const StorageOptions& options,
    scoped_refptr<QueuesContainer> queues_container,
    scoped_refptr<EncryptionModuleInterface> encryption_module,
    scoped_refptr<CompressionModule> compression_module,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    base::OnceCallback<void(StatusOr<scoped_refptr<Storage>>)> completion_cb) {
  // Initializes Storage object and populates all the queues by reading the
  // storage directory and parsing queue directory names. Deletes directories
  // that do not following the queue directory name format.
  class StorageInitContext
      : public TaskRunnerContext<StatusOr<scoped_refptr<Storage>>> {
   public:
    StorageInitContext(
        scoped_refptr<Storage> storage,
        base::OnceCallback<void(StatusOr<scoped_refptr<Storage>>)> callback)
        : TaskRunnerContext<StatusOr<scoped_refptr<Storage>>>(
              std::move(callback),
              storage->sequenced_task_runner_),  // Same runner as the Storage!
          storage_(std::move(storage)) {}

   private:
    // Context can only be deleted by calling Response method.
    ~StorageInitContext() override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);
      CHECK_EQ(count_, 0u);
    }

    void OnStart() override {
      CheckOnValidSequence();

      const bool executed_without_error =
          StorageDirectory::DeleteEmptyMultigenerationQueueDirectories(
              storage_->options_.directory());
      LOG_IF(WARNING, executed_without_error)
          << "Errors while deleting empty directories";

      // Get the information we need to create queues
      DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);
      queue_parameters_ = StorageDirectory::FindQueueDirectories(
          storage_->options_.directory(),
          storage_->options_.ProduceQueuesOptionsList());

      // If encryption is not enabled, proceed with the queues.
      if (!storage_->encryption_module_->is_enabled()) {
        InitAllQueues();
        return;
      }

      // Encryption is enabled. Locate the latest signed_encryption_key
      // file with matching key signature after deserialization.
      const auto download_key_result =
          storage_->key_in_storage_->DownloadKeyFile();
      if (!download_key_result.has_value()) {
        // Key not found or corrupt. Proceed with encryption setup.
        // Key will be downloaded during setup.
        EncryptionSetUp(download_key_result.error());
        return;
      }

      // Key found, verified and downloaded.
      storage_->encryption_module_->UpdateAsymmetricKey(
          download_key_result.value().first, download_key_result.value().second,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &StorageInitContext::EncryptionSetUp, base::Unretained(this))));
    }

    void EncryptionSetUp(Status status) {
      CheckOnValidSequence();

      if (status.ok()) {
        // Encryption key has been found and set up. Must be available now.
        CHECK(storage_->encryption_module_->has_encryption_key());
        // Enable periodic updates of the key.
        storage_->key_delivery_->StartPeriodicKeyUpdate();
      } else {
        LOG(WARNING)
            << "Encryption is enabled, but the key is not available yet, "
               "status="
            << status;
      }

      InitAllQueues();
    }

    void InitAllQueues() {
      CheckOnValidSequence();

      DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);
      count_ = queue_parameters_.size();
      if (count_ == 0) {
        Response(std::move(storage_));
        return;
      }

      // Create queues the queue directories we found in the storage directory
      for (const auto& [priority, generation_guid] : queue_parameters_) {
        Start<CreateQueueContext>(
            // Don't transfer ownership of  `storage_` via std::move() since
            // we need to return `storage_` in the response
            priority, storage_->options_.ProduceQueueOptions(priority),
            storage_, generation_guid,
            base::BindOnce(&StorageInitContext::QueueCreated,
                           base::Unretained(this)),
            base::BindPostTaskToCurrentDefault(
                base::BindOnce(&StorageInitContext::RespondIfAllQueuesCreated,
                               base::Unretained(this))));
      }
    }

    void QueueCreated(scoped_refptr<StorageQueue> created_queue,
                      base::OnceCallback<void(Status)> completion_cb) {
      CheckOnValidSequence();
      std::move(completion_cb).Run(Status::StatusOK());
    }

    void RespondIfAllQueuesCreated(Status status) {
      CheckOnValidSequence();
      DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);
      if (!status.ok()) {
        LOG(ERROR) << "Failed to create queue during Storage creation, error="
                   << status;
        final_status_ = status;
      }
      CHECK_GT(count_, 0u);
      if (--count_ > 0u) {
        return;
      }
      if (!final_status_.ok()) {
        Response(base::unexpected(final_status_));
        return;
      }
      Response(std::move(storage_));
    }

    StorageOptions::QueuesOptionsList queues_options_
        GUARDED_BY_CONTEXT(storage_->sequence_checker_);
    const scoped_refptr<Storage> storage_;
    size_t count_ GUARDED_BY_CONTEXT(storage_->sequence_checker_) = 0;
    Status final_status_ GUARDED_BY_CONTEXT(storage_->sequence_checker_) =
        Status::StatusOK();
    // Stores necessary fields for creating queues. Populated by parsing queue
    // directory names.
    StorageDirectory::Set queue_parameters_
        GUARDED_BY_CONTEXT(storage_->sequence_checker_);
  };

  // Create Storage object.
  // Cannot use base::MakeRefCounted<Storage>, because constructor is
  // private.
  auto storage = base::WrapRefCounted(
      new Storage(options, queues_container, encryption_module,
                  compression_module, async_start_upload_cb));

  // Asynchronously run initialization.
  Start<StorageInitContext>(std::move(storage), std::move(completion_cb));
}

Storage::Storage(const StorageOptions& options,
                 scoped_refptr<QueuesContainer> queues_container,
                 scoped_refptr<EncryptionModuleInterface> encryption_module,
                 scoped_refptr<CompressionModule> compression_module,
                 UploaderInterface::AsyncStartUploaderCb async_start_upload_cb)
    : options_(options),
      sequenced_task_runner_(queues_container->sequenced_task_runner()),
      encryption_module_(encryption_module),
      key_delivery_(KeyDelivery::Create(options_.key_check_period(),
                                        encryption_module,
                                        async_start_upload_cb)),
      compression_module_(compression_module),
      key_in_storage_(std::make_unique<KeyInStorage>(
          options.signature_verification_public_key(),
          options.directory())),
      async_start_upload_cb_(async_start_upload_cb),
      queues_container_(queues_container) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Storage::~Storage() = default;

void Storage::Write(Priority priority,
                    Record record,
                    base::OnceCallback<void(Status)> completion_cb) {
  // Ensure everything is executed on Storage's sequenced task runner
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<Storage> self, Priority priority, Record record,
             base::OnceCallback<void(Status)> completion_cb) {
            const DMtoken& dm_token = record.dm_token();

            // Callback that writes to the queue.
            auto write_queue_action =
                base::BindOnce(&Storage::WriteToQueue, self, std::move(record));

            GenerationGuid generation_guid;
            if (self->options_.is_multi_generational(priority)) {
              // Get or create the generation guid associated with the dm token
              // and priority in this record.
              StatusOr<GenerationGuid> generation_guid_result =
                  self->queues_container_->GetOrCreateGenerationGuid(dm_token,
                                                                     priority);

              if (!generation_guid_result.has_value()) {
                // This should never happen. We should always be able to create
                // a generation guid if one doesn't exist.
                NOTREACHED_NORETURN();
              }
              generation_guid = generation_guid_result.value();
            }

            // Find the queue for this generation guid + priority and write to
            // it.
            auto queue_result = self->TryGetQueue(priority, generation_guid);
            if (queue_result.has_value()) {
              // The queue we need already exists, so we can write to it.
              std::move(write_queue_action)
                  .Run(std::move(queue_result.value()),
                       std::move(completion_cb));
              return;
            }
            // We don't have a queue for this priority + generation guid, so
            // create one, and then let the context execute the write
            // via `write_queue_action`. Note that we can end up in a race
            // with another `Write` of the same `priority` and
            // `generation_guid`, and in that case only one queue will survive
            // and be used.
            Start<CreateQueueContext>(
                priority, self->options_.ProduceQueueOptions(priority), self,
                generation_guid, std::move(write_queue_action),
                std::move(completion_cb));
          },
          base::WrapRefCounted(this), priority, std::move(record),
          std::move(completion_cb)));
}

void Storage::WriteToQueue(Record record,
                           scoped_refptr<StorageQueue> queue,
                           base::OnceCallback<void(Status)> completion_cb) {
  if (encryption_module_->is_enabled() &&
      !encryption_module_->has_encryption_key()) {
    // Key was not found at startup time. Note that if the key
    // is outdated, we still can use it, and won't load it now.
    // So this processing can only happen after Storage is
    // initialized (until the first successful delivery of a
    // key). After that we will resume the write into the queue.
    KeyDelivery::RequestCallback action = base::BindOnce(
        [](scoped_refptr<StorageQueue> queue, Record record,
           base::OnceCallback<void(Status)> completion_cb, Status status) {
          if (!status.ok()) {
            std::move(completion_cb).Run(status);
            return;
          }
          queue->Write(std::move(record), std::move(completion_cb));
        },
        queue, std::move(record), std::move(completion_cb));
    key_delivery_->Request(std::move(action));
    return;
  }
  // Otherwise we can write into the queue right away.
  queue->Write(std::move(record), std::move(completion_cb));
}

void Storage::Confirm(SequenceInformation sequence_information,
                      bool force,
                      base::OnceCallback<void(Status)> completion_cb) {
  // Subtle bug: sequence_information is moved instead of copied, so we need
  // to extract fields from it, or else those fields  will be empty when
  // sequence_information is consumed by std::move
  const GenerationGuid generation_guid = sequence_information.generation_guid();
  const Priority priority = sequence_information.priority();

  // Prepare an async confirmation action to be directed to the queue.
  auto queue_confirm_action = base::BindOnce(
      [](SequenceInformation sequence_information, bool force,
         scoped_refptr<StorageQueue> queue,
         base::OnceCallback<void(Status)> completion_cb) {
        queue->Confirm(std::move(sequence_information), force,
                       std::move(completion_cb));
      },
      std::move(sequence_information), force);
  // Locate or create a queue, pass it to the action callback.
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<Storage> self, Priority priority,
             StatusOr<GenerationGuid> generation_guid,
             base::OnceCallback<void(scoped_refptr<StorageQueue>,
                                     base::OnceCallback<void(Status)>)>
                 queue_action,
             base::OnceCallback<void(Status)> completion_cb) {
            auto queue_result = self->TryGetQueue(priority, generation_guid);
            if (!queue_result.has_value()) {
              std::move(completion_cb).Run(queue_result.error());
              return;
            }
            // Queue found, execute the action (it should relocate on
            // queue thread soon, to not block Storage task runner).
            std::move(queue_action)
                .Run(queue_result.value(), std::move(completion_cb));
          },
          base::WrapRefCounted(this), priority, std::move(generation_guid),
          std::move(queue_confirm_action), std::move(completion_cb)));
}

class FlushContext : public TaskRunnerContext<Status> {
 public:
  FlushContext(scoped_refptr<Storage> storage,
               Priority priority,
               base::OnceCallback<void(Status)> callback)
      : TaskRunnerContext<Status>(
            std::move(callback),
            storage->sequenced_task_runner_),  // Same runner as the Storage!
        storage_(storage),
        priority_(priority) {}

  FlushContext(const FlushContext&) = delete;
  FlushContext& operator=(const FlushContext&) = delete;

 private:
  // Context can only be deleted by calling Response method.
  ~FlushContext() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);
    CHECK_EQ(count_, 0u);
  }

  void OnStart() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);

    // Flush each queue
    count_ = storage_->queues_container_->RunActionOnAllQueues(
        priority_,
        base::BindRepeating(
            [](FlushContext* context, scoped_refptr<StorageQueue> queue) {
              queue->Flush(base::BindPostTaskToCurrentDefault(base::BindOnce(
                  &FlushContext::RespondIfAllQueuesAreFlush,
                  base::Unretained(context), queue->generation_guid())));
            },
            base::Unretained(this)));
  }

  void RespondIfAllQueuesAreFlush(GenerationGuid generation_guid,
                                  Status status) {
    CheckOnValidSequence();
    DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);

    if (!status.ok()) {
      if (final_status_.ok()) {
        final_status_ = status;
      }
      LOG(ERROR) << "Failed to flush queue with priority = " << priority_
                 << " generation_guid=" << generation_guid
                 << ", error=" << status.error_message();
    }
    CHECK_GT(count_, 0u);
    if (--count_ > 0u) {
      return;
    }
    Response(final_status_);
  }

  Status final_status_ GUARDED_BY_CONTEXT(storage_->sequence_checker_) =
      Status::StatusOK();
  const scoped_refptr<Storage> storage_;
  size_t count_ GUARDED_BY_CONTEXT(storage_->sequence_checker_) = 0;
  const Priority priority_;
};

void Storage::Flush(Priority priority,
                    base::OnceCallback<void(Status)> completion_cb) {
  // If key is not available, there is nothing to flush, but we need to
  // request the key instead.
  if (encryption_module_->is_enabled() &&
      !encryption_module_->has_encryption_key()) {
    key_delivery_->Request(std::move(completion_cb));
    return;
  }

  Start<FlushContext>(base::WrapRefCounted(this), priority,
                      std::move(completion_cb));
}

void Storage::UpdateEncryptionKey(SignedEncryptionInfo signed_encryption_key) {
  // Verify received key signature. Bail out if failed.
  const auto signature_verification_status =
      key_in_storage_->VerifySignature(signed_encryption_key);
  if (!signature_verification_status.ok()) {
    LOG(WARNING) << "Key failed verification, status="
                 << signature_verification_status;
    key_delivery_->OnKeyUpdateResult(signature_verification_status);
    return;
  }

  // Assign the received key to encryption module.
  encryption_module_->UpdateAsymmetricKey(
      signed_encryption_key.public_asymmetric_key(),
      signed_encryption_key.public_key_id(),
      base::BindOnce(
          [](scoped_refptr<Storage> storage, Status status) {
            if (!status.ok()) {
              LOG(WARNING) << "Encryption key update failed, status=" << status;
              storage->key_delivery_->OnKeyUpdateResult(status);
              return;
            }
            // Encryption key updated successfully.
            storage->key_delivery_->OnKeyUpdateResult(Status::StatusOK());
          },
          base::WrapRefCounted(this)));

  // Serialize whole signed_encryption_key to a new file, discard the old
  // one(s). Do it on a thread which may block doing file operations.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](SignedEncryptionInfo signed_encryption_key,
             scoped_refptr<Storage> storage) {
            const Status status =
                storage->key_in_storage_->UploadKeyFile(signed_encryption_key);
            LOG_IF(ERROR, !status.ok())
                << "Failed to upload the new encription key.";
          },
          std::move(signed_encryption_key), base::WrapRefCounted(this)));
}

StatusOr<scoped_refptr<StorageQueue>> Storage::TryGetQueue(
    Priority priority,
    StatusOr<GenerationGuid> generation_guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!generation_guid.has_value()) {
    return base::unexpected(std::move(generation_guid).error());
  }
  // Attempt to get queue by priority and generation_guid on
  // the Storage task runner.
  auto queue_result =
      queues_container_->GetQueue(priority, generation_guid.value());
  if (!queue_result.has_value()) {
    // Queue not found, abort.
    return base::unexpected(std::move(queue_result).error());
  }
  // Queue found, return it.
  return std::move(queue_result).value();
}

void Storage::RegisterCompletionCallback(base::OnceClosure callback) {
  // Although this is an asynchronous action, note that Storage cannot be
  // destructed until the callback is registered - StorageQueue is held by
  // added reference here. Thus, the callback being registered is guaranteed
  // to be called when the Storage is being destructed.
  CHECK(callback);
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&QueuesContainer::RegisterCompletionCallback,
                                queues_container_, std::move(callback)));
}
}  // namespace reporting

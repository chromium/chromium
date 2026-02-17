// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/delta_patch_operation.h"

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/patcher.h"
#include "components/update_client/pipeline_util.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"

namespace update_client {

DeltaPatchOperation::DeltaPatchOperation(
    scoped_refptr<CrxCache> crx_cache,
    base::RepeatingCallback<void(base::DictValue)> event_adder,
    base::RepeatingCallback<void(ComponentState)> state_tracker,
    const std::string& old_hash,
    uint32_t new_file_flags,
    const std::string& output_hash,
    int expected_success_result,
    const base::FilePath& patch_file,
    int event_type,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback)
    : crx_cache_(crx_cache),
      event_adder_(event_adder),
      state_tracker_(state_tracker),
      old_hash_(old_hash),
      new_file_flags_(new_file_flags),
      output_hash_(output_hash),
      expected_success_result_(expected_success_result),
      patch_file_(patch_file),
      event_type_(event_type),
      callback_(std::move(callback)) {}

DeltaPatchOperation::~DeltaPatchOperation() = default;

void DeltaPatchOperation::Operation(
    base::OnceCallback<
        void(base::File /*old_file*/,
             base::File /*patch_file*/,
             base::File /*output_file*/,
             base::OnceCallback<void(int)> /*completion_callback*/)> strategy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  strategy_ = std::move(strategy);
  state_tracker_.Run(ComponentState::kPatching);

  crx_cache_->GetByHash(
      old_hash_, base::BindOnce(&DeltaPatchOperation::CacheLookupDone, this));
}

void DeltaPatchOperation::CacheLookupDone(
    base::expected<base::FilePath, UnpackerError> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, kTaskTraits,
        base::BindOnce(
            [](const base::FilePath& patch_file) {
              DeleteFileAndEmptyParentDirectory(patch_file);
            },
            patch_file_),
        base::BindOnce(&DeltaPatchOperation::PatchDone, this,
                       base::unexpected<CategorizedError>(
                           {.category = ErrorCategory::kUnpack,
                            .code = std::to_underlying(result.error())})));
    return;
  }

  base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits)
      ->PostTask(FROM_HERE, base::BindOnce(&DeltaPatchOperation::Patch, this,
                                           result.value()));
}

void DeltaPatchOperation::Patch(const base::FilePath& old_file) {
  base::FilePath new_file =
      patch_file_.DirName().Append(FILE_PATH_LITERAL("delta_patch_out"));

  std::move(strategy_).Run(
      base::File(old_file, base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(patch_file_, base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(new_file, new_file_flags_),
      base::BindOnce(&DeltaPatchOperation::VerifyAndCleanUp, this, new_file));
}

void DeltaPatchOperation::VerifyAndCleanUp(const base::FilePath& new_file,
                                           int result) {
  RetryFileOperation(&base::DeleteFile, patch_file_);

  const auto expected_filepath =
      [&] -> base::expected<base::FilePath, CategorizedError> {
    if (result != expected_success_result_) {
      return base::unexpected<CategorizedError>(
          {.category = ErrorCategory::kUnpack,
           .code = std::to_underlying(UnpackerError::kDeltaOperationFailure),
           .extra = result});
    }

    if (!VerifyFileHash256(new_file, output_hash_)) {
      return base::unexpected<CategorizedError>(
          {.category = ErrorCategory::kUnpack,
           .code = std::to_underlying(UnpackerError::kPatchOutHashMismatch)});
    }

    return new_file;
  }();

  if (!expected_filepath.has_value()) {
    DeleteFileAndEmptyParentDirectory(new_file);
  }

  original_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeltaPatchOperation::PatchDone, this, expected_filepath));
}

void DeltaPatchOperation::PatchDone(
    base::expected<base::FilePath, CategorizedError> result) {
  event_adder_.Run(MakeSimpleOperationEvent(result, event_type_));
  std::move(callback_).Run(result);
}

}  // namespace update_client

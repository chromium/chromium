// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_puffin.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/patcher.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "third_party/puffin/src/include/puffin/puffpatch.h"

namespace update_client {

namespace {

// The sequence of calls is:
//
// [Original Sequence]    [Blocking Pool]
//
// PuffOperation
// CacheLookupDone
//                        Patch
//                        Cleanup
// PatchDone
// [original callback]
//
// All errors shortcut to PatchDone.

// Runs on the original sequence. Adds events and calls the original callback.
void PatchDone(
    base::OnceCallback<
        void(const base::expected<base::FilePath, CategorizedError>&)> callback,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    const base::expected<base::FilePath, CategorizedError>& result) {
  // TODO(crbug.com/353249967): Add an event describing the patch's outcome.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

// Runs in the blocking pool. Deletes any files that are no longer needed.
void CleanUp(
    base::OnceCallback<
        void(const base::expected<base::FilePath, CategorizedError>&)> callback,
    const base::FilePath& patch_file,
    const base::FilePath& new_file,
    int result) {
  base::DeleteFile(patch_file);
  if (result != puffin::P_OK) {
    base::DeleteFile(new_file);
    std::move(callback).Run(base::unexpected<CategorizedError>(
        {.category_ = ErrorCategory::kUnpack,
         .code_ = static_cast<int>(UnpackerError::kDeltaOperationFailure),
         .extra_ = result}));
    return;
  }
  std::move(callback).Run(new_file);
}

// Runs in the blocking pool. Opens file handles and applies the patch.
void Patch(
    scoped_refptr<Patcher> patcher,
    const base::FilePath& old_file,
    const base::FilePath& patch_file,
    const base::FilePath& temp_dir,
    base::OnceCallback<void(
        const base::expected<base::FilePath, CategorizedError>&)> callback) {
  base::FilePath new_file = temp_dir.AppendASCII("puffpatch_out");
  patcher->PatchPuffPatch(
      base::File(old_file, base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(patch_file, base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(new_file, base::File::FLAG_CREATE_ALWAYS |
                               base::File::FLAG_WRITE |
                               base::File::FLAG_WIN_EXCLUSIVE_WRITE),
      base::BindOnce(&CleanUp, std::move(callback), patch_file, new_file));
}

// Runs on the original sequence.
void CacheLookupDone(
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    scoped_refptr<Patcher> patcher,
    const base::FilePath& patch_file,
    const base::FilePath& temp_dir,
    base::OnceCallback<
        void(const base::expected<base::FilePath, CategorizedError>&)> callback,
    const CrxCache::Result& cache_result) {
  if (cache_result.error != UnpackerError::kNone) {
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, kTaskTraits,
        base::BindOnce(IgnoreResult(&base::DeleteFile), patch_file),
        base::BindOnce(&PatchDone, std::move(callback), event_adder,
                       base::unexpected<CategorizedError>(
                           {.category_ = ErrorCategory::kUnpack,
                            .code_ = static_cast<int>(cache_result.error)})));
    return;
  }
  base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&Patch, patcher, cache_result.crx_cache_path,
                         patch_file, temp_dir,
                         base::BindPostTaskToCurrentDefault(base::BindOnce(
                             &PatchDone, std::move(callback), event_adder))));
}

}  // namespace

void PuffOperation(
    std::optional<scoped_refptr<CrxCache>> crx_cache,
    scoped_refptr<Patcher> patcher,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    const std::string& id,
    const std::string& prev_fp,
    const base::FilePath& patch_file,
    const base::FilePath& temp_dir,
    base::OnceCallback<void(
        const base::expected<base::FilePath, CategorizedError>&)> callback) {
  if (!crx_cache) {
    CrxCache::Result result;
    result.error = UnpackerError::kCrxCacheNotProvided;
    CacheLookupDone(event_adder, patcher, patch_file, temp_dir,
                    std::move(callback), result);
    return;
  }

  crx_cache.value()->Get(
      id, prev_fp,
      base::BindOnce(&CacheLookupDone, event_adder, patcher, patch_file,
                     temp_dir, std::move(callback)));
}

}  // namespace update_client

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_DELTA_PATCH_OPERATION_H_
#define COMPONENTS_UPDATE_CLIENT_DELTA_PATCH_OPERATION_H_

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/update_client.h"

namespace update_client {

class CrxCache;
struct CategorizedError;

// `DeltaPatchOperation` encapsulates the common logic for applying delta
// patches (puffin or zucchini). It handles cache lookups, file management,
// background execution, and sending event pings.
//
// The sequence of calls is:
//
// [Original Sequence]    [Blocking Pool]
//
// Operation
// CacheLookupDone
//                        Patch
//                        VerifyAndCleanUp
// PatchDone
//
// All errors shortcut to PatchDone.
class DeltaPatchOperation
    : public base::RefCountedThreadSafe<DeltaPatchOperation> {
 public:
  // Created on the original sequence.
  DeltaPatchOperation(
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
          callback);

  // Runs on the original sequence. Starts the operation.
  void Operation(
      base::OnceCallback<void(
          base::File /*old_file*/,
          base::File /*patch_file*/,
          base::File /*output_file*/,
          base::OnceCallback<void(int)> /*completion_callback*/)> strategy);

 private:
  friend class base::RefCountedThreadSafe<DeltaPatchOperation>;
  virtual ~DeltaPatchOperation();

  // Runs on the original sequence. Shortcuts to `PatchDone` on error, otherwise
  // calls `Patch` on a blocking sequenced task runner.
  void CacheLookupDone(base::expected<base::FilePath, UnpackerError> result);

  // Runs on a blocking sequenced task runner. Calls `VerifyAndCleanUp` once
  // done.
  void Patch(const base::FilePath& old_file_path);

  // Runs on a blocking sequenced task runner. Calls `PatchDone` on the original
  // sequence after verification and cleanup.
  void VerifyAndCleanUp(const base::FilePath& new_file_path, int result);

  // Runs on the original sequence. Sends event pings and calls the final
  // `callback_`.
  void PatchDone(base::expected<base::FilePath, CategorizedError> result);

  // Bound to the original sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<CrxCache> crx_cache_;
  base::RepeatingCallback<void(base::DictValue)> event_adder_;
  base::RepeatingCallback<void(ComponentState)> state_tracker_;
  const std::string old_hash_;
  const uint32_t new_file_flags_;
  const std::string output_hash_;
  const int expected_success_result_;
  const base::FilePath patch_file_;
  const int event_type_;
  base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
      callback_;
  scoped_refptr<base::SequencedTaskRunner> original_sequence_ =
      base::SequencedTaskRunner::GetCurrentDefault();

  base::OnceCallback<void(
      base::File /*old_file*/,
      base::File /*patch_file*/,
      base::File /*output_file*/,
      base::OnceCallback<void(int)> /*completion_callback*/)>
      strategy_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_DELTA_PATCH_OPERATION_H_

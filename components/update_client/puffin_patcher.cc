// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/puffin_patcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "components/update_client/component_patcher_operation.h"
#include "components/update_client/patcher.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "third_party/puffin/src/include/puffin/puffpatch.h"

namespace update_client {

PuffinPatcher::PuffinPatcher(
    base::File old_crx_file,
    base::File puff_patch_file,
    base::File new_crx_output,
    scoped_refptr<Patcher> patcher,
    base::OnceCallback<void(UnpackerError, int)> callback)
    : old_crx_file_(std::move(old_crx_file)),
      puff_patch_file_(std::move(puff_patch_file)),
      new_crx_output_file_(std::move(new_crx_output)),
      patcher_(patcher),
      callback_(std::move(callback)) {}

PuffinPatcher::~PuffinPatcher() = default;

void PuffinPatcher::Patch(
    base::File old_crx_file,
    base::File puff_patch_file,
    base::File new_crx_output,
    scoped_refptr<Patcher> patcher,
    base::OnceCallback<void(UnpackerError, int)> callback) {
  scoped_refptr<PuffinPatcher> puffin_patcher =
      base::WrapRefCounted(new PuffinPatcher(
          std::move(old_crx_file), std::move(puff_patch_file),
          std::move(new_crx_output), patcher, std::move(callback)));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PuffinPatcher::StartPatching, puffin_patcher));
}

void PuffinPatcher::StartPatching() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!old_crx_file_.IsValid()) {
    DonePatching(UnpackerError::kInvalidParams, 0);
  } else if (!puff_patch_file_.IsValid()) {
    DonePatching(UnpackerError::kInvalidParams, 0);
  } else if (!new_crx_output_file_.IsValid()) {
    DonePatching(UnpackerError::kInvalidParams, 0);
  } else {
    PatchCrx();
  }
}

void PuffinPatcher::PatchCrx() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  patcher_->PatchPuffPatch(std::move(old_crx_file_),
                           std::move(puff_patch_file_),
                           std::move(new_crx_output_file_),
                           base::BindOnce(&PuffinPatcher::DonePatch, this));
}

void PuffinPatcher::DonePatch(int extended_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (extended_error != puffin::P_OK) {
    DonePatching(UnpackerError::kDeltaOperationFailure, extended_error);
  } else {
    DonePatching(UnpackerError::kNone, 0);
  }
}

void PuffinPatcher::DonePatching(UnpackerError error, int extended_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  std::move(callback_).Run(error, extended_error);
}

}  // namespace update_client

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/cancellation.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"

namespace update_client {

Cancellation::Cancellation() = default;
Cancellation::~Cancellation() = default;

void Cancellation::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelWithMode(CancelMode::kDeletePartialDownload);
}

void Cancellation::CancelWithMode(CancelMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cancelled_) {
    return;
  }
  cancelled_ = true;
  cancel_mode_ = mode;
  if (task_) {
    std::move(task_).Run(cancel_mode_);
  }
}

bool Cancellation::IsCancelled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cancelled_;
}

void Cancellation::OnCancel(CancelCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cancelled_) {
    if (callback) {
      std::move(callback).Run(cancel_mode_);
    }
  } else {
    task_ = std::move(callback);
  }
}

void Cancellation::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_.Reset();
}

}  // namespace update_client

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/cancellation.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"

namespace update_client {

Cancellation::Cancellation() = default;
Cancellation::~Cancellation() = default;

void Cancellation::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancelled_ = true;
  std::move(task_).Run();
}

bool Cancellation::IsCancelled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cancelled_;
}

void Cancellation::OnCancel(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsCancelled()) {
    std::move(task).Run();
  } else {
    task_ = std::move(task);
  }
}

void Cancellation::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_ = base::DoNothing();
}

}  // namespace update_client

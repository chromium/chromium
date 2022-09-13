// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_buffer_submitter.h"

#include "components/autofill/core/browser/logging/log_router.h"

namespace autofill {

LogBufferSubmitter::LogBufferSubmitter(LogRouter* destination, bool active)
    : destination_(destination),
      buffer_(LogBuffer::IsActive(destination != nullptr && active)),
      destruct_with_logging_(buffer_.active()) {}

LogBufferSubmitter::LogBufferSubmitter(LogBufferSubmitter&& that) noexcept
    : destination_(std::move(that.destination_)),
      buffer_(std::move(that.buffer_)),
      destruct_with_logging_(std::move(that.destruct_with_logging_)) {
  that.destruct_with_logging_ = false;
}

LogBufferSubmitter& LogBufferSubmitter::operator=(LogBufferSubmitter&& that) {
  destination_ = std::move(that.destination_);
  buffer_ = std::move(that.buffer_);
  destruct_with_logging_ = std::move(that.destruct_with_logging_);
  that.destruct_with_logging_ = false;
  return *this;
}

LogBufferSubmitter::~LogBufferSubmitter() {
  if (!destruct_with_logging_ || !destination_)
    return;
  absl::optional<base::Value::Dict> message = buffer_.RetrieveResult();
  if (!message)
    return;
  destination_->ProcessLog(*message);
}

}  // namespace autofill

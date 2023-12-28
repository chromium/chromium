// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_buffer_submitter.h"

#include "components/autofill/core/browser/logging/log_manager.h"

namespace autofill {

LogBufferSubmitter::LogBufferSubmitter(LogManager* log_manager)
    : log_manager_(log_manager),
      buffer_(IsLoggingActive(log_manager)),
      destruct_with_logging_(buffer_.active()) {}

LogBufferSubmitter::LogBufferSubmitter(LogBufferSubmitter&& that) noexcept
    : log_manager_(std::move(that.log_manager_)),
      buffer_(std::move(that.buffer_)),
      destruct_with_logging_(std::move(that.destruct_with_logging_)) {
  that.destruct_with_logging_ = false;
}

LogBufferSubmitter& LogBufferSubmitter::operator=(LogBufferSubmitter&& that) {
  log_manager_ = std::move(that.log_manager_);
  buffer_ = std::move(that.buffer_);
  destruct_with_logging_ = std::move(that.destruct_with_logging_);
  that.destruct_with_logging_ = false;
  return *this;
}

LogBufferSubmitter::~LogBufferSubmitter() {
  if (!destruct_with_logging_ || !log_manager_)
    return;
  std::optional<base::Value::Dict> message = buffer_.RetrieveResult();
  if (!message)
    return;
  log_manager_->ProcessLog(std::move(*message), {});
}

}  // namespace autofill

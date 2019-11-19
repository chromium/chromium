// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_buffer_submitter.h"

#include "components/autofill/core/browser/logging/log_router.h"

namespace autofill {

LogBufferSubmitter::LogBufferSubmitter(LogRouter* destination, bool active)
    : destination_(destination) {
  buffer_.set_active(destination != nullptr && active);
}

LogBufferSubmitter::LogBufferSubmitter(LogBufferSubmitter&& that) noexcept =
    default;

LogBufferSubmitter::~LogBufferSubmitter() {
  base::Value message = buffer_.RetrieveResult();
  if (!destination_ || message.is_none())
    return;
  destination_->ProcessLog(std::move(message));
}

}  // namespace autofill

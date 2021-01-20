// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_BUFFER_SUBMITTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_BUFFER_SUBMITTER_H_

#include "base/macros.h"
#include "components/autofill/core/common/logging/log_buffer.h"

namespace autofill {

class LogRouter;

// A container for a LogBuffer that submits the buffer to the passed destination
// on destruction.
//
// Use it in the following way:
// LogBufferSubmitter(destination) << "Foobar";
// The submitter is destroyed after this statement and "Foobar" is logged.
class LogBufferSubmitter {
 public:
  LogBufferSubmitter(LogRouter* destination, bool active);
  LogBufferSubmitter(LogBufferSubmitter&& that) noexcept;
  ~LogBufferSubmitter();

  LogBuffer& buffer() { return buffer_; }
  operator LogBuffer&() { return buffer_; }

 private:
  LogRouter* destination_;
  LogBuffer buffer_;
  DISALLOW_COPY_AND_ASSIGN(LogBufferSubmitter);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_BUFFER_SUBMITTER_H_

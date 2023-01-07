// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_BUFFER_SUBMITTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_BUFFER_SUBMITTER_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/common/logging/log_buffer.h"

namespace autofill {

class LogManager;

// A container for a LogBuffer that submits the buffer to the passed destination
// on destruction.
//
// Use it in the following way:
// LogBufferSubmitter(destination) << "Foobar";
// The submitter is destroyed after this statement and "Foobar" is logged.
class LogBufferSubmitter {
 public:
  explicit LogBufferSubmitter(LogManager* manager);
  ~LogBufferSubmitter();

  LogBufferSubmitter(LogBufferSubmitter&& that) noexcept;
  LogBufferSubmitter& operator=(LogBufferSubmitter&& that);

  LogBufferSubmitter(LogBufferSubmitter& that) = delete;
  LogBufferSubmitter& operator=(LogBufferSubmitter& that) = delete;

  LogBuffer& buffer() { return buffer_; }
  operator LogBuffer&() { return buffer_; }

 private:
  raw_ptr<LogManager> log_manager_;
  LogBuffer buffer_;
  // If set to false, the destructor does not perform any logging. This is used
  // for move assignment so that the original copy does not trigger logging.
  bool destruct_with_logging_ = true;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_BUFFER_SUBMITTER_H_

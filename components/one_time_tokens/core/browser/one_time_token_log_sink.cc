// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"

namespace one_time_tokens {

OneTimeTokenLogSink::OneTimeTokenLogSink() = default;
OneTimeTokenLogSink::~OneTimeTokenLogSink() = default;

base::CallbackListSubscription OneTimeTokenLogSink::AddLogHandler(
    LogHandler handler) {
  return callback_list_.Add(std::move(handler));
}

bool OneTimeTokenLogSink::IsActive() const {
  return !callback_list_.empty();
}

void OneTimeTokenLogSink::Emit(std::string_view message) {
  callback_list_.Notify(message);
}

LogMessageBuffer::LogMessageBuffer(OneTimeTokenLogSink* sink) : sink_(sink) {}

LogMessageBuffer::~LogMessageBuffer() {
  if (sink_) {
    sink_->Emit(stream_.str());
  }
}

}  // namespace one_time_tokens

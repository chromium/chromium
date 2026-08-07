// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_LOG_SINK_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_LOG_SINK_H_

#include <sstream>
#include <string_view>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace one_time_tokens {

class OneTimeTokenLogSink {
 public:
  using LogHandler = base::RepeatingCallback<void(std::string_view)>;

  OneTimeTokenLogSink();
  ~OneTimeTokenLogSink();

  [[nodiscard]] base::CallbackListSubscription AddLogHandler(
      LogHandler handler);

  bool IsActive() const;

  void Emit(std::string_view message);

 private:
  base::RepeatingCallbackList<void(std::string_view)> callback_list_;
};

class LogMessageBuffer {
 public:
  explicit LogMessageBuffer(OneTimeTokenLogSink* sink);
  ~LogMessageBuffer();

  template <typename T>
  LogMessageBuffer& operator<<(const T& value) {
    stream_ << value;
    return *this;
  }

 private:
  raw_ptr<OneTimeTokenLogSink> sink_;
  std::ostringstream stream_;
};

class Voidify {
 public:
  constexpr Voidify() = default;
  template <typename U>
  void operator&(const U&) {}
};

#define LOG_OTT(sink)                  \
  !((sink) && (sink)->IsActive())      \
      ? (void)0                        \
      : ::one_time_tokens::Voidify() & \
            ::one_time_tokens::LogMessageBuffer((sink))

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_LOG_SINK_H_

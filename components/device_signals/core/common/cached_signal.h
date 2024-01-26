// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_CACHED_SIGNAL_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_CACHED_SIGNAL_H_

#include <optional>

#include "base/time/time.h"

namespace device_signals {

template <typename T>
class CachedSignal {
 public:
  CachedSignal(base::TimeDelta expiry_period) : expiry_period_(expiry_period) {}
  CachedSignal(const CachedSignal&) = delete;
  CachedSignal& operator=(const CachedSignal&) = delete;
  ~CachedSignal() = default;

  const std::optional<T>& Get() {
    if (base::TimeTicks::Now() - timestamp_ >= expiry_period_) {
      cached_value_ = std::nullopt;
    }
    return cached_value_;
  }

  void Set(const T& value) {
    cached_value_ = value;
    timestamp_ = base::TimeTicks::Now();
  }

 private:
  const base::TimeDelta expiry_period_;

  std::optional<T> cached_value_ = std::nullopt;
  base::TimeTicks timestamp_;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_CACHED_SIGNAL_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MEMORY_HINT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MEMORY_HINT_H_

#include <optional>

#include "base/time/time.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {

// A representation of a hint to be used by in-memory caches.
class MemoryHint {
 public:
  MemoryHint(const std::optional<base::Time>& expiry_time,
             std::unique_ptr<proto::Hint> hint);
  MemoryHint(base::Time expiry_time, proto::Hint&& hint);
  MemoryHint(const MemoryHint&) = delete;
  MemoryHint& operator=(const MemoryHint&) = delete;
  ~MemoryHint();

  // This value is not available if |hint_| was sourced from the Optimization
  // Hints component. Otherwise, it is set.
  const std::optional<base::Time> expiry_time() const { return expiry_time_; }
  // It is the responsibility of the callers to avoid use-after-free references
  // that may occur if the containing object is evicted from an in-memory
  // cache.
  optimization_guide::proto::Hint* hint() const { return hint_.get(); }

 private:
  std::optional<base::Time> expiry_time_;
  std::unique_ptr<proto::Hint> hint_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MEMORY_HINT_H_

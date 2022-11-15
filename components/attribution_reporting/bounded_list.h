// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_BOUNDED_LIST_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_BOUNDED_LIST_H_

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

template <typename T, size_t kMaxSize>
class BoundedList {
 public:
  static absl::optional<BoundedList> Create(std::vector<T> vec) {
    if (vec.size() > kMaxSize)
      return absl::nullopt;

    return BoundedList(std::move(vec));
  }

  BoundedList() = default;

  const std::vector<T>& vec() const { return vec_; }

 private:
  explicit BoundedList(std::vector<T> vec) : vec_(std::move(vec)) {
    DCHECK_LE(vec_.size(), kMaxSize);
  }

  std::vector<T> vec_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_BOUNDED_LIST_H_

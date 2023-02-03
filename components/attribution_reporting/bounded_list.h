// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_BOUNDED_LIST_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_BOUNDED_LIST_H_

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/function_ref.h"
#include "base/types/expected.h"
#include "base/values.h"
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

  template <typename Error>
  static base::expected<BoundedList, Error> Build(
      base::Value* input_value,
      Error wrong_type,
      Error out_of_bounds,
      base::FunctionRef<base::expected<T, Error>(base::Value&)> build_element) {
    if (!input_value)
      return BoundedList();

    base::Value::List* list = input_value->GetIfList();
    if (!list)
      return base::unexpected(wrong_type);

    if (list->size() > kMaxSize)
      return base::unexpected(out_of_bounds);

    std::vector<T> vec;
    vec.reserve(list->size());

    for (auto& value : *list) {
      base::expected<T, Error> element = build_element(value);
      if (!element.has_value())
        return base::unexpected(element.error());

      vec.push_back(std::move(*element));
    }

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

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_INLINE_DATA_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_INLINE_DATA_IMPL_H_

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "base/check.h"

namespace performance_manager {

template <class T>
class SparseNodeInlineData;

namespace internal {

// Store the element as a std::optional<T>. Used for non-sparse
// NodeInlineData<T>.
template <class T>
class OptionalStorage {
 public:
  bool Exists() { return data_.has_value(); }

  template <class... Args>
  T& Create(Args&&... args) {
    CHECK(!data_);
    return data_.emplace(std::forward<Args>(args)...);
  }

  void Destroy() {
    CHECK(data_);
    data_.reset();
  }

  T& Get() {
    CHECK(data_);
    return *data_;
  }

  const T& Get() const {
    CHECK(data_);
    return *data_;
  }

 private:
  std::optional<T> data_;
};

// Store the element in a std::unique_ptr<T>. Used for SparseNodeInlineData<T>.
template <class T>
class SparseStorage {
 public:
  bool Exists() { return data_.get(); }

  template <class... Args>
  T& Create(Args&&... args) {
    CHECK(!data_);
    data_ = std::make_unique<T>(std::forward<Args>(args)...);
    return *data_;
  }

  void Destroy() {
    CHECK(data_);
    data_.reset();
  }

  T& Get() {
    CHECK(data_);
    return *data_;
  }

  const T& Get() const {
    CHECK(data_);
    return *data_;
  }

 private:
  std::unique_ptr<T> data_;
};

template <typename T>
concept IsSparseNodeInlineData = std::derived_from<T, SparseNodeInlineData<T>>;

template <class T>
using Storage = std::conditional_t<IsSparseNodeInlineData<T>,
                                   SparseStorage<T>,
                                   OptionalStorage<T>>;

}  // namespace internal

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_INLINE_DATA_IMPL_H_

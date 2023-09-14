// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_INSERTION_ORDERED_SET_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_INSERTION_ORDERED_SET_H_

#include <vector>

#include "base/containers/flat_set.h"

namespace optimization_guide {

// Keeps a set of unique element, while preserving the insertion order. vector()
// can be accessed to get the ordered elements.
template <typename T>
class InsertionOrderedSet {
 public:
  InsertionOrderedSet() = default;

  void insert(const T& elem) {
    auto ret = set_.insert(elem);
    if (ret.second) {
      // The element wasn't already in the container.
      vector_.push_back(elem);
    }
  }

  void clear() {
    set_.clear();
    vector_.clear();
  }

  bool empty() const { return vector_.empty(); }

  size_t size() const { return vector_.size(); }

  const std::vector<T>& vector() const { return vector_; }

  const base::flat_set<T>& set() const { return set_; }

 private:
  base::flat_set<T> set_;
  std::vector<T> vector_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_INSERTION_ORDERED_SET_H_

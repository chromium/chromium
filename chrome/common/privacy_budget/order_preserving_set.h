// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRIVACY_BUDGET_ORDER_PRESERVING_SET_H_
#define CHROME_COMMON_PRIVACY_BUDGET_ORDER_PRESERVING_SET_H_

#include <type_traits>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/stl_util.h"

// A combination of a set and a list. Lookup semantics come from the set,
// iterator semantics come from the list.
//
// Iterators preserve the insertion or initialization order.
//
// Implementing Container, ReversibleContainer, AssociativeContainer are
// non-goals. It has the bare minimum functionality required for the use cases
// in this directory.
//
// It is assumed that there will be waay more lookups than mutations. Otherwise
// the underlying container choices may not be efficient.
template <typename T>
class OrderPreservingSet {
 public:
  using value_type = typename std::remove_cv_t<T>;
  using set_type = base::flat_set<value_type>;
  using list_type = std::vector<value_type>;
  using const_iterator = typename list_type::const_iterator;

  OrderPreservingSet() = default;

  // Initializes this object with the contents of `base_list` whose elements
  // *MUST* be distinct. Preserves the order of elements in `base_list`.
  explicit OrderPreservingSet(list_type&& base_list)
      : list_(std::move(base_list)), set_(list_.begin(), list_.end()) {
    DCHECK(CheckModel());
  }

  template <typename V>
  OrderPreservingSet(std::initializer_list<V> v)
      : list_(v), set_(list_.begin(), list_.end()) {}

  // Modifiers:
  template <typename V>
  bool Add(V&& v) {
    auto insertion_result = set_.insert(std::forward<V>(v));
    if (insertion_result.second)
      list_.push_back(v);
    return insertion_result.second;
  }

  template <typename V>
  void push_back(V&& v) {
    Add(std::forward<V>(v));
  }

  void Clear() {
    base::STLClearObject(&list_);
    base::STLClearObject(&set_);
  }

  void reserve(typename list_type::size_type element_count) {
    list_.reserve(element_count);
    set_.reserve(element_count);
  }

  // Assign the contents of `list` to this object. Elements of `list` *MUST* be
  // distinct. Preserves the order of elements in `list`.
  template <typename V>
  OrderPreservingSet<T>& Assign(V&& list) {
    list_ = std::forward<V>(list);
    set_type replacement_set(list_.begin(), list_.end());
    set_.swap(replacement_set);
    DCHECK(CheckModel());
    return *this;
  }

  // Assign the contents of `list` to this object. Elements of `list` *MUST* be
  // distinct. Preserves the order of elements in `list`.
  template <typename V>
  OrderPreservingSet<T>& operator=(V&& list) {
    return Assign(std::forward<V>(list));
  }

  // Inspectors:
  template <typename V>
  auto contains(V&& v) const {
    return set_.contains(std::forward<V>(v));
  }
  // Note the absence of a find() method which would need to return
  // a set_type::iterator that will clash with the list_type::iterator values
  // returned by the iterators below.

  // Forwards to list_type.
  auto size() const { return list_.size(); }
  bool empty() const { return list_.empty(); }
  value_type operator[](typename list_type::size_type index) const {
    return list_[index];
  }

  // Iteration is in order of insertion. Mutable iterators are not supported.
  // Hence everything is const only.
  const_iterator begin() const { return list_.begin(); }
  const_iterator end() const { return list_.end(); }
  const_iterator rbegin() const { return list_.rbegin(); }
  const_iterator rend() const { return list_.rend(); }

  const list_type& AsList() const { return list_; }

 private:
#if DCHECK_IS_ON()
  // Model checking.
  bool CheckModel() const {
    set_type normalized(list_.begin(), list_.end());
    return normalized == set_ && list_.size() == set_.size();
  }
#else
  bool CheckModel() const { return true; }
#endif

  list_type list_;
  set_type set_;
};

#endif  // CHROME_COMMON_PRIVACY_BUDGET_ORDER_PRESERVING_SET_H_

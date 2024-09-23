// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_SET_VIEW_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_SET_VIEW_H_

#include <iterator>
#include <type_traits>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "components/performance_manager/public/graph/node.h"

namespace performance_manager {

// A view over a NodeSet. Provides an iterable view where elements are casted to
// the requested type using NodeView::FromNode.
template <class UnderlyingSet, class NodeViewPtr>
class NodeSetView {
 public:
  using value_type = NodeViewPtr;

  using NodeView = std::remove_pointer_t<NodeViewPtr>;

  class Iterator {
   public:
    using value_type = NodeViewPtr;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    // Need a default constructor to pass the static_assert below.
    Iterator() { NOTREACHED(); }

    explicit Iterator(UnderlyingSet::const_iterator iterator)
        : iterator_(iterator) {}

    value_type operator*() const { return NodeView::FromNode(*iterator_); }

    Iterator& operator++() {
      ++iterator_;
      return *this;
    }

    Iterator operator++(int) {
      Iterator old(*this);
      ++iterator_;
      return old;
    }

    bool operator==(const Iterator& other) const {
      return iterator_ == other.iterator_;
    }

   private:
    UnderlyingSet::const_iterator iterator_;
  };

  // Make sure the implementation conforms to a standard forward iterator.
  static_assert(std::forward_iterator<Iterator>);

  explicit NodeSetView(const UnderlyingSet& node_set) : node_set_(node_set) {}

  ~NodeSetView() = default;

  // Standard iterator interface.
  Iterator begin() const { return Iterator(node_set_->begin()); }
  Iterator end() const { return Iterator(node_set_->end()); }

  bool empty() const { return node_set_->empty(); }

  size_t size() const { return node_set_->size(); }

  // Helper method to generate a vector containing all the elements.
  std::vector<NodeViewPtr> AsVector() {
    return std::vector<NodeViewPtr>(begin(), end());
  }

 private:
  raw_ref<const UnderlyingSet> node_set_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_SET_VIEW_H_

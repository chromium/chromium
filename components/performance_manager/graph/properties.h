// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_

#include <utility>

#include "base/check.h"
#include "components/performance_manager/public/graph/node_state.h"

namespace performance_manager {

// Helper classes for setting properties and invoking observer callbacks based
// on the value change. This is templated on the observer type to allow
// easy testing.
//
// Objects of NodeImplType are expected to fulfill the contract:
//
//   IterableCollection GetObservers() const;
//   bool NodeImplType::CanSetProperty() const;
//   bool NodeImplType::CanSetAndNotifyProperty() const;
//
// These are used in DCHECKs to assert that properties are only being modified
// at appropriate moments. If your code is blowing up in one of these DCHECKS
// you are trying to change a property while a node is being added or removed
// from the graph. When adding to the graph property changes should be done in a
// separate posted task. When removing from the graph, they should simply not be
// done. See class comments on node observers, NodeState, and NodeBase for full
// details.
template <typename NodeImplType, typename NodeType, typename ObserverType>
class ObservedPropertyImpl {
 public:
  // Helper class for node properties that represent measurements that are taken
  // periodically, and for which a notification should be sent every time a
  // new sample is recorded, even if identical in value to the last.
  template <typename PropertyType,
            void (ObserverType::*NotifyFunctionPtr)(const NodeType*)>
  class NotifiesAlways {
   public:
    NotifiesAlways() = default;

    template <typename U = PropertyType>
    explicit NotifiesAlways(U&& initial_value)
        : value_(std::forward<U>(initial_value)) {}

    ~NotifiesAlways() = default;

    // Sets the property and sends a notification.
    template <typename U = PropertyType>
    void SetAndNotify(NodeImplType* node, U&& value) {
      // If your code is blowing up here see the class comment!
      DCHECK(node->CanSetAndNotifyProperty());
      value_ = std::forward<U>(value);
      for (auto& observer : node->GetObservers()) {
        (observer.*NotifyFunctionPtr)(node);
      }
    }

    // Sets the property without sending a notification.
    template <typename U = PropertyType>
    void Set(NodeImplType* node, U&& value) {
      // If your code is blowing up here see the class comment!
      DCHECK(node->CanSetProperty());
      value_ = std::forward<U>(value);
    }

    const PropertyType& value() const { return value_; }

   private:
    PropertyType value_;
  };

  // Helper class for node properties that represent states, for which
  // notifications should only be sent when the value of the property actually
  // changes. Calls to SetAndMaybeNotify do not notify if the provided value is
  // the same as the current value.
  template <typename PropertyType,
            void (ObserverType::*NotifyFunctionPtr)(const NodeType*)>
  class NotifiesOnlyOnChanges {
   public:
    NotifiesOnlyOnChanges() = default;

    template <typename U = PropertyType>
    explicit NotifiesOnlyOnChanges(U&& initial_value)
        : value_(std::forward<U>(initial_value)) {}

    ~NotifiesOnlyOnChanges() = default;

    // Sets the property and sends a notification if needed. Returns true if a
    // notification was sent, false otherwise.
    template <typename U = PropertyType>
    bool SetAndMaybeNotify(NodeImplType* node, U&& value) {
      // If your code is blowing up here see the class comment!
      DCHECK(node->CanSetAndNotifyProperty());
      if (value_ == value)
        return false;
      value_ = std::forward<U>(value);
      for (auto& observer : node->GetObservers()) {
        (observer.*NotifyFunctionPtr)(node);
      }
      return true;
    }

    // Sets the property without sending a notification.
    template <typename U = PropertyType>
    void Set(NodeImplType* node, U&& value) {
      // If your code is blowing up here see the class comment!
      DCHECK(node->CanSetProperty());
      value_ = std::forward<U>(value);
    }

    const PropertyType& value() const { return value_; }

   private:
    PropertyType value_;
  };

  // Same as NotifiesOnlyOnChanges, but provides the previous value when
  // notifying observers.
  // TODO(chrisha): When C++17 is available, deduce PreviousValueType via use of
  // an 'auto' placeholder non-type template parameter helper.
  template <typename PropertyType,
            typename PreviousValueType,
            void (ObserverType::*NotifyFunctionPtr)(
                const NodeType* node,
                PreviousValueType previous_value)>
  class NotifiesOnlyOnChangesWithPreviousValue {
   public:
    NotifiesOnlyOnChangesWithPreviousValue() = default;

    template <typename U = PropertyType>
    explicit NotifiesOnlyOnChangesWithPreviousValue(U&& initial_value)
        : value_(std::forward<U>(initial_value)) {}

    ~NotifiesOnlyOnChangesWithPreviousValue() = default;

    // Sets the property and sends a notification if needed. Returns true if a
    // notification was sent, false otherwise.
    template <typename U = PropertyType>
    bool SetAndMaybeNotify(NodeImplType* node, U&& value) {
      // If your code is blowing up here see the class comment!
      DCHECK(node->CanSetAndNotifyProperty());
      if (value_ == value)
        return false;
      PropertyType previous_value = std::move(value_);
      value_ = std::forward<U>(value);
      for (auto& observer : node->GetObservers()) {
        (observer.*NotifyFunctionPtr)(node, previous_value);
      }
      return true;
    }

    // Sets the property without sending a notification.
    template <typename U = PropertyType>
    void Set(NodeImplType* node, U&& value) {
      // If your code is blowing up here see the class comment!
      DCHECK(node->CanSetProperty());
      value_ = std::forward<U>(value);
    }

    const PropertyType& value() const { return value_; }

   private:
    PropertyType value_;
  };
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_

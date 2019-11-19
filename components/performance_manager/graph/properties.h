// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_

#include <utility>

namespace performance_manager {

// TODO(chrisha): Deprecate the private observer type and have everyone use the
// public observers!

// Helper classes for setting properties and invoking observer callbacks based
// on the value change. Note that by contract the NodeType must have a member
// function "observers()" that returns an iterable collection of
// ObserverType pointers. This is templated on the observer type to allow
// easy testing.
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
      value_ = std::forward<U>(value);
      for (auto* observer : node->GetObservers())
        ((observer)->*(NotifyFunctionPtr))(node);
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
      if (value_ == value)
        return false;
      value_ = std::forward<U>(value);
      for (auto* observer : node->GetObservers())
        ((observer)->*(NotifyFunctionPtr))(node);
      return true;
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
      if (value_ == value)
        return false;
      PropertyType previous_value = std::move(value_);
      value_ = std::forward<U>(value);
      for (auto* observer : node->GetObservers())
        ((observer)->*(NotifyFunctionPtr))(node, previous_value);
      return true;
    }

    const PropertyType& value() const { return value_; }

   private:
    PropertyType value_;
  };
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_

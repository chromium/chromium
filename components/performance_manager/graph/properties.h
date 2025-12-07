// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_

#include <utility>

#include "base/check.h"
#include "base/trace_event/trace_event.h"
#include "components/performance_manager/graph/tracing_observer.h"
#include "components/performance_manager/public/graph/node_state.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace performance_manager {

perfetto::StaticString YesNoStateToString(const bool& is_yes);

template <typename PropertyType>
class TracedWrapper : public TracingObserver {
 public:
  using ConverterFuncPtr = perfetto::StaticString (*)(const PropertyType&);

  TracedWrapper(perfetto::NamedTrack track, ConverterFuncPtr converter_func)
      : track_(track), converter_func_(converter_func) {
    Trace();
    if (auto* observer_list = TracingObserverList::GetFromGraph()) {
      observer_list->AddObserver(this);
    }
  }

  template <typename U = PropertyType>
  TracedWrapper(U&& initial_value,
                perfetto::NamedTrack track,
                ConverterFuncPtr converter_func)
      : value_(std::forward<U>(initial_value)),
        track_(track),
        converter_func_(converter_func) {
    Trace();
    if (auto* observer_list = TracingObserverList::GetFromGraph()) {
      observer_list->AddObserver(this);
    }
  }

  ~TracedWrapper() override {
    if (auto* observer_list = TracingObserverList::GetFromGraph()) {
      observer_list->RemoveObserver(this);
    }
    if (slice_is_open_) {
      TRACE_EVENT_END("performance_manager.graph", track_);
    }
  }

  template <typename U = PropertyType>
  void Set(U&& value) {
    value_ = std::forward<U>(value);
    Trace();
  }

  void OnTraceSessionStart() override { Trace(); }

  // Ref-Qualifier is specified explicitly to allow std::move(wrapper).value().
  const PropertyType& value() const& { return value_; }
  PropertyType&& value() && { return std::move(value_); }

  perfetto::StaticString ToString() const { return converter_func_(value_); }

 private:
  void Trace() {
    if (!TRACE_EVENT_CATEGORY_ENABLED("performance_manager.graph")) {
      return;
    }
    if (slice_is_open_) {
      TRACE_EVENT_END("performance_manager.graph", track_);
      slice_is_open_ = false;
    }
    perfetto::StaticString value_str = converter_func_(value_);
    if (value_str) {
      TRACE_EVENT_BEGIN("performance_manager.graph", value_str, track_);
      slice_is_open_ = true;
    }
  }

  PropertyType value_;
  perfetto::NamedTrack track_;
  ConverterFuncPtr converter_func_;
  bool slice_is_open_ = false;
};

template <typename PropertyType>
class TrivialWrapper {
 public:
  TrivialWrapper() = default;

  template <typename U = PropertyType>
  explicit TrivialWrapper(U&& initial_value)
      : value_(std::forward<U>(initial_value)) {}

  template <typename U = PropertyType>
  void Set(U&& value) {
    value_ = std::forward<U>(value);
  }

  const PropertyType& value() const& { return value_; }
  PropertyType&& value() && { return std::move(value_); }

 private:
  PropertyType value_;
};

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
            void (ObserverType::*NotifyFunctionPtr)(const NodeType*),
            class Wrapper = TrivialWrapper<PropertyType>>
  class NotifiesAlways {
   public:
    NotifiesAlways() = default;

    template <typename... Args>
    explicit NotifiesAlways(Args&&... args)
        : value_(std::forward<Args>(args)...) {}

    ~NotifiesAlways() = default;

    // Sets the property and sends a notification.
    template <typename U = PropertyType>
    void SetAndNotify(NodeImplType* node, U&& value) {
      // If your code is blowing up here see the class comment!
      DCHECK(node->CanSetAndNotifyProperty());
      value_.Set(std::forward<U>(value));
      for (auto& observer : node->GetObservers()) {
        (observer.*NotifyFunctionPtr)(node);
      }
    }

    // Sets the property without sending a notification.
    template <typename U = PropertyType>
    void Set(NodeImplType* node, U&& value) {
      // If your code is blowing up here see the class comment!
      DCHECK(node->CanSetProperty());
      value_.Set(std::forward<U>(value));
    }

    const PropertyType& value() const { return value_.value(); }

   private:
    Wrapper value_;
  };

  // Helper class for node properties that represent states, for which
  // notifications should only be sent when the value of the property actually
  // changes. Calls to SetAndMaybeNotify do not notify if the provided value is
  // the same as the current value.
  template <typename PropertyType,
            void (ObserverType::*NotifyFunctionPtr)(const NodeType*),
            class Wrapper = TrivialWrapper<PropertyType>>
  class NotifiesOnlyOnChanges {
   public:
    NotifiesOnlyOnChanges() = default;

    template <typename... Args>
    explicit NotifiesOnlyOnChanges(Args&&... args)
        : value_(std::forward<Args>(args)...) {}

    ~NotifiesOnlyOnChanges() = default;

    // Sets the property and sends a notification if needed. Returns true if a
    // notification was sent, false otherwise.
    template <typename U = PropertyType>
    bool SetAndMaybeNotify(NodeImplType* node, U&& value) {
      // If your code is blowing up here see the class comment!
      DCHECK(node->CanSetAndNotifyProperty());
      if (value_.value() == value) {
        return false;
      }
      value_.Set(std::forward<U>(value));
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
      value_.Set(std::forward<U>(value));
    }

    const PropertyType& value() const { return value_.value(); }

   private:
    Wrapper value_;
  };

  // Same as NotifiesOnlyOnChanges, but provides the previous value when
  // notifying observers.
  template <typename PropertyType,
            auto NotifyFunctionPtr,
            class Wrapper = TrivialWrapper<PropertyType>>
  class NotifiesOnlyOnChangesWithPreviousValue {
   public:
    NotifiesOnlyOnChangesWithPreviousValue() = default;

    template <typename... Args>
    explicit NotifiesOnlyOnChangesWithPreviousValue(Args&&... args)
        : value_(std::forward<Args>(args)...) {}

    ~NotifiesOnlyOnChangesWithPreviousValue() = default;

    // Sets the property and sends a notification if needed. Returns true if a
    // notification was sent, false otherwise.
    template <typename U = PropertyType>
    bool SetAndMaybeNotify(NodeImplType* node, U&& value) {
      // If your code is blowing up here see the class comment!
      DCHECK(node->CanSetAndNotifyProperty());
      if (value_.value() == value) {
        return false;
      }
      PropertyType previous_value = std::move(value_).value();
      value_.Set(std::forward<U>(value));
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
      value_.Set(std::forward<U>(value));
    }

    const PropertyType& value() const { return value_.value(); }
    const Wrapper* operator->() const { return &value_; }

   private:
    Wrapper value_;
  };
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROPERTIES_H_

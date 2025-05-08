// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_SUPPORTS_HANDLES_H_
#define COMPONENTS_TABS_PUBLIC_SUPPORTS_HANDLES_H_

// SupportsHandles is a way to add "handle" support to an object, such as a tab
// or browser window which:
//  - may be transient (i.e. the reference could later become invalid)
//  - needs to be safely referenced from code in other languages, such as
//    JavaScript code in extensions
//
// If you do not need *both* of these, consider a raw or weak pointer instead.
//
// A handle is a semi-opaque value that is safe to store and pass around even
// after the underlying object is destroyed. They behave more or less like weak
// pointers, but have the added benefit that they contain an integral
// `raw_value` which can be copied around, and even passed between programming
// languages.
//
// To use handles with a class, inherit publicly from
// `SupportsHandles<YourClassName>`. Then a handle can be retrieved from an
// instance, and the instance retrieved from the handle:
// ```
//  MyClass::Handle handle = my_object->GetHandle();
//  // Do a bunch of stuff that might delete `my_object`.
//  if (MyClass* obj = handle.Get()) {
//    obj->DoAThing();
//  }
// ```
//
// Notes:
//
// Handle values do not persist across process restart (though restoring handle
// values at startup could be implemented in some future iteration of this
// library.)
//
// Objects with handles may only be generated and retrieved on the primary UI
// thread, though their handles and handle values may be copied to and stored on
// any thread.
//
// Handles are backed by a 32-bit signed integer, since (a) there are usually
// not more than 4 billion of any UI object, and (b) signed integers provide the
// broadest possible language support.
//
// The null value for handles is always zero.
//
// It is a fatal error to try to create a new object if all int32_t values have
// already been used. If an object is likely to run through all possible values
// (that is, have more than 4 billion constructed over the life of a chrome
// instance) then it is probably a poor candidate for handles.

#include <concepts>
#include <cstdint>
#include <map>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"

namespace tabs {

// Inherit from this type to have your class support handles. Objects that
// support handles cannot be copyable or assignable:
// ```
// class C : public SupportsHandles<C> { ... }
// ```
//
// It is required that `T` derive from this class. This constraint is enforced
// via a helper class, as it cannot be enforced before SupportsHandles is
// defined.
template <typename T>
class SupportsHandles {
 public:
  SupportsHandles();
  virtual ~SupportsHandles();
  SupportsHandles(const SupportsHandles& other) = delete;
  void operator=(const SupportsHandles& other) = delete;

  // The handle type for this class.
  class Handle;

  // Returns a unique handle value for this object.
  Handle GetHandle() const;

 private:
  int32_t handle_value_;
};

// The handle type for an object of type `T`.
//
// This is a default-constructable, orderable, comparable, copyable value type.
//
// Unlike WeakPtr there is some overhead in looking up a handle, so convenience
// operators (bool, !, ->, *) are not provided.
template <typename T>
class SupportsHandles<T>::Handle {
 public:
  Handle() = default;
  Handle(const Handle& other) = default;
  ~Handle() = default;
  Handle& operator=(const Handle& other) = default;

  // The object type returned by `Get()`.
  using ObjectType = T;

  // Convert to/from a raw, opaque handle value. It is safe to pass this value
  // around, including to code running in other languages.
  explicit Handle(int32_t raw_value) : raw_value_(raw_value) {}
  int32_t raw_value() const { return raw_value_; }

  // Retrieves the underlying object, or null if it is no longer present.
  ObjectType* Get() const;

  // Handles are comparable and sortable.
  friend bool operator==(const Handle&, const Handle&) = default;
  friend auto operator<=>(const Handle&, const Handle&) = default;

  // Explicitly provide the null value and handle.
  static constexpr int32_t NullValue = 0;
  static Handle Null() { return Handle(NullValue); }

 private:
  int32_t raw_value_ = NullValue;
};

class SupportsHandlesTest;

namespace internal {

// Provides handle lookup table storage for each class that supports handles.
//
// This object is strictly sequence-checked and should only ever be accessed
// from the primary UI thread.
template <typename T>
  requires std::derived_from<T, SupportsHandles<T>>
class HandleHelper {
 public:
  using StoredPointerType = SupportsHandles<T>*;

  // Assigns a new, unused handle value for `object` and returns the value.
  // Called from the constructor of `SupportsHandles`.
  int32_t AssignHandleValue(StoredPointerType object) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    CHECK(object);

    // Use the next available handle value; it is an error if the value rolls
    // back over to zero.
    ++last_handle_value_;
    CHECK(last_handle_value_)
        << "Fatal handle reuse! Please curtail object creation.";

    lookup_table_.emplace(last_handle_value_, object);
    return last_handle_value_;
  }

  // Frees a handle with `handle_value`. Must be called from the destructor of
  // `SupportsHandles`.
  void FreeHandleValue(int32_t handle_value) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    CHECK(lookup_table_.erase(handle_value));
  }

  // Retrieves the object associated with the given `handle_value`, or null if
  // no such object exists.
  T* LookupObject(int32_t handle_value) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    const auto it = lookup_table_.find(handle_value);
    return it != lookup_table_.end() ? static_cast<T*>(it->second) : nullptr;
  }

  // The lookup object is a singleton per `SupportsHandle`-derived type.
  static HandleHelper& GetInstance() {
    static base::NoDestructor<HandleHelper> instance;
    return *instance;
  }

 private:
  friend SupportsHandlesTest;
  friend class base::NoDestructor<HandleHelper<T>>;
  HandleHelper() = default;
  ~HandleHelper() = default;

  int32_t last_handle_value_ GUARDED_BY_CONTEXT(sequence_) = 0;
  std::map<int32_t, StoredPointerType> lookup_table_
      GUARDED_BY_CONTEXT(sequence_);
  SEQUENCE_CHECKER(sequence_);
};

}  // namespace internal

template <typename T>
SupportsHandles<T>::SupportsHandles()
    : handle_value_(
          internal::HandleHelper<T>::GetInstance().AssignHandleValue(this)) {}

template <typename T>
SupportsHandles<T>::~SupportsHandles() {
  internal::HandleHelper<T>::GetInstance().FreeHandleValue(handle_value_);
}

template <typename T>
typename SupportsHandles<T>::Handle SupportsHandles<T>::GetHandle() const {
  return Handle(handle_value_);
}

template <typename T>
T* SupportsHandles<T>::Handle::Get() const {
  return internal::HandleHelper<T>::GetInstance().LookupObject(raw_value_);
}

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_SUPPORTS_HANDLES_H_

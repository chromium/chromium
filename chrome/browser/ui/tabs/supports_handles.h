// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SUPPORTS_HANDLES_H_
#define CHROME_BROWSER_UI_TABS_SUPPORTS_HANDLES_H_

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
// The default raw handle type `V` is a 32-bit signed integer, since (a) there
// are usually not more than 4 billion of any UI object, and (b) signed integers
// provide the broadest possible language support. However, you may choose any
// integral type for `V`.
//
// Regardless of choice of `V`, the null value for handles is always zero.
//
// It is a fatal error to try to create a new object if all valid values of `V`
// have already been used. If an object is likely to run through all possible
// values of V (that is, have more than 4 billion constructed over the life of
// a chrome instance) then it is probably a poor candidate for handles (or at
// the very least, you need to pick a larger V).
//
// TODO(dfried, tbergquist): move this file down to c/b/ui if it's used outside
// of the tabstrip.

#include <concepts>
#include <cstdint>
#include <map>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"

// Inherit from this type to have your class support handles. Objects that
// support handles cannot be copyable or assignable:
// ```
// class C : public SupportsHandles<C> { ... }
// ```
//
// `V` defines the type (and therefore size in bits) of the underlying handle
// value. A wider value type will provide more unique handle values.
//
// It is required that `T` derive from this class. This constraint is enforced
// via a helper class, as it cannot be enforced before SupportsHandles is
// defined.
template <typename T, std::integral V = int32_t>
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
  V handle_value_;
};

// The handle type for an object of type `T`.
//
// This is a default-constructable, orderable, comparable, copyable value type.
//
// Unlike WeakPtr there is some overhead in looking up a handle, so convenience
// operators (bool, !, ->, *) are not provided.
template <typename T, std::integral V>
class SupportsHandles<T, V>::Handle {
 public:
  Handle() = default;
  Handle(const Handle& other) = default;
  ~Handle() = default;
  Handle& operator=(const Handle& other) = default;

  // The underlying opaque handle value type.
  using RawValueType = V;

  // The object type returned by `Get()`.
  using ObjectType = T;

  // Convert to/from a raw, opaque handle value. It is safe to pass this value
  // around, including to code running in other languages.
  explicit Handle(RawValueType raw_value) : raw_value_(raw_value) {}
  RawValueType raw_value() const { return raw_value_; }

  // Retrieves the underlying object, or null if it is no longer present.
  ObjectType* Get() const;

  // Handles are comparable and sortable.
  bool operator==(const Handle& other) const {
    return raw_value_ == other.raw_value_;
  }
  bool operator!=(const Handle& other) const {
    return raw_value_ != other.raw_value_;
  }
  bool operator<(const Handle& other) const {
    return raw_value_ < other.raw_value_;
  }

  // Explicitly provide the null value and handle.
  static constexpr RawValueType NullValue = 0;
  static Handle Null() { return Handle(NullValue); }

 private:
  RawValueType raw_value_ = NullValue;
};

class SupportsHandlesTest;

namespace internal {

// Provides handle lookup table storage for each class that supports handles.
//
// This object is strictly sequence-checked and should only ever be accessed
// from the primary UI thread.
template <typename T, std::integral V>
  requires std::derived_from<T, SupportsHandles<T, V>>
class HandleHelper {
 public:
  using StoredPointerType = SupportsHandles<T, V>*;

  // Assigns a new, unused handle value for `object` and returns the value.
  // Called from the constructor of `SupportsHandles`.
  V AssignHandleValue(StoredPointerType object) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    CHECK(object);

    // Use the next available handle value; it is an error if the value rolls
    // back over to zero.
    ++last_handle_value_;
    CHECK(last_handle_value_)
        << "Fatal handle reuse! Please use a larger raw value type (V) or "
           "curtail object creation.";

    lookup_table_.emplace(last_handle_value_, object);
    return last_handle_value_;
  }

  // Frees a handle with `handle_value`. Must be called from the destructor of
  // `SupportsHandles`.
  void FreeHandleValue(V handle_value) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    CHECK(lookup_table_.erase(handle_value));
  }

  // Retrieves the object associated with the given `handle_value`, or null if
  // no such object exists.
  T* LookupObject(V handle_value) const {
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
  friend class base::NoDestructor<HandleHelper<T, V>>;
  HandleHelper() = default;
  ~HandleHelper() = default;

  V last_handle_value_ GUARDED_BY_CONTEXT(sequence_) = 0;
  std::map<V, StoredPointerType> lookup_table_ GUARDED_BY_CONTEXT(sequence_);
  SEQUENCE_CHECKER(sequence_);
};

}  // namespace internal

template <typename T, std::integral V>
SupportsHandles<T, V>::SupportsHandles()
    : handle_value_(
          internal::HandleHelper<T, V>::GetInstance().AssignHandleValue(this)) {
}

template <typename T, std::integral V>
SupportsHandles<T, V>::~SupportsHandles() {
  internal::HandleHelper<T, V>::GetInstance().FreeHandleValue(handle_value_);
}

template <typename T, std::integral V>
typename SupportsHandles<T, V>::Handle SupportsHandles<T, V>::GetHandle()
    const {
  return Handle(handle_value_);
}

template <typename T, std::integral V>
T* SupportsHandles<T, V>::Handle::Get() const {
  return internal::HandleHelper<T, V>::GetInstance().LookupObject(raw_value_);
}

#endif  // CHROME_BROWSER_UI_TABS_SUPPORTS_HANDLES_H_

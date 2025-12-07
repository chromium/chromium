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
// To use handles with a class, it must inherit from `SupportsHandles<Factory>`
// where `Factory` is a class that provides handle storage and lookup.
//
// For a default factory, which is sufficient for most cases:
//  - Use `DECLARE_HANDLE_FACTORY(MyClass)` just before your class declaration.
//  - Inherit from `SupportsHandles<MyClassHandleFactory>`.
//  - In the corresponding .cc file, use `DEFINE_HANDLE_FACTORY(MyClass)`.
//
// For more complex hierarchies of objects, `DECLARE_BASE_HANDLE_FACTORY()` can
// be used to create a base factory class to derive from.
//
// Example:
// ```
//  // in the .h file
//  DECLARE_HANDLE_FACTORY(TabThingy);
//  class TabThingy : public SupportsHandles<TabThingyHandleFactory> { ... };
//
//  // in the .cc file
//  DEFINE_HANDLE_FACTORY(TabThingy);
// ```
//
// Once you have done this, a handle can be retrieved from an instance, and the
// instance can be retrieved from the handle:
// ```
//  MyClass::Handle handle = my_object->GetHandle();
//  // Do a bunch of stuff that might or might not delete `my_object`.
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
#include <ostream>

#include "base/check.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"

namespace tabs {

class SupportsHandlesTest;

namespace internal {

class HandleFactory;

// Base class for all objects that support handles.
class SupportsHandlesBase {
 public:
  virtual ~SupportsHandlesBase();
  SupportsHandlesBase(const SupportsHandlesBase& other) = delete;
  void operator=(const SupportsHandlesBase& other) = delete;

 protected:
  explicit SupportsHandlesBase(HandleFactory& factory);

  // Looks up the object with `handle_value` in `factory`.
  static SupportsHandlesBase* LookupHandle(HandleFactory& factory,
                                           int32_t handle_value);

 protected:
  int32_t handle_value() const { return handle_value_; }

 private:
  const raw_ref<HandleFactory> handle_factory_;
  const int32_t handle_value_;
};

// Provides handle lookup table storage for each class that supports handles.
//
// This object is strictly sequence-checked and should only ever be accessed
// from the primary UI thread.
class HandleFactory {
 public:
  HandleFactory(HandleFactory&) = delete;
  void operator=(HandleFactory&) = delete;
  virtual ~HandleFactory();

  using StoredPointerType = SupportsHandlesBase*;

  // Assigns a new, unused handle value for `object` and returns the value.
  // Called from the constructor of `SupportsHandles`.
  int32_t AssignHandleValue(base::PassKey<SupportsHandlesBase>,
                            StoredPointerType object);

  // Frees a handle with `handle_value`. Must be called from the destructor of
  // `SupportsHandles`.
  void FreeHandleValue(base::PassKey<SupportsHandlesBase>,
                       int32_t handle_value);

  // Retrieves the object associated with the given `handle_value`, or null if
  // no such object exists.
  StoredPointerType LookupObject(base::PassKey<SupportsHandlesBase>,
                                 int32_t handle_value) const;

 protected:
  HandleFactory();

#if DCHECK_IS_ON()
  const base::SequenceChecker& sequence() const { return sequence_; }
#endif  // DCHECK_IS_ON()

 private:
  friend SupportsHandlesTest;

  // Invoked when a handle is no longer mapped to an object.
  virtual void OnHandleFreed(int32_t handle_value) {}

  int32_t last_handle_value_ GUARDED_BY_CONTEXT(sequence_) = 0;
  std::map<int32_t, StoredPointerType> lookup_table_
      GUARDED_BY_CONTEXT(sequence_);

  SEQUENCE_CHECKER(sequence_);
};

// Created to force type safety.
template <typename T>
class HandleFactoryT : public HandleFactory {
 public:
  using ObjectType = T;

  ~HandleFactoryT() override = default;

 protected:
  HandleFactoryT() = default;
};

template <typename Factory>
concept IsHandleFactory =
    std::derived_from<Factory,
                      internal::HandleFactoryT<typename Factory::ObjectType>> &&
    requires {
      { Factory::GetInstance() } -> std::same_as<Factory&>;
    };

}  // namespace internal

// Inherit from this type to have your class support handles. Objects that
// support handles cannot be copyable or assignable:
// ```
// class C : public SupportsHandles<C> { ... }
// ```
//
// It is required that `T` derive from this class. This constraint is enforced
// via a helper class, as it cannot be enforced before SupportsHandles is
// defined.
template <typename Factory>
  requires internal::IsHandleFactory<Factory>
class SupportsHandles : public internal::SupportsHandlesBase {
 public:
  SupportsHandles() : SupportsHandlesBase(Factory::GetInstance()) {}
  ~SupportsHandles() override = default;

  // The handle type for this class.
  class Handle;

  // Returns a unique handle value for this object.
  Handle GetHandle() const;
};

// The handle type for an object of type `T`.
//
// This is a default-constructable, orderable, comparable, copyable value type.
//
// Unlike WeakPtr there is some overhead in looking up a handle, so convenience
// operators (bool, !, ->, *) are not provided.
template <typename Factory>
  requires internal::IsHandleFactory<Factory>
class SupportsHandles<Factory>::Handle {
 public:
  Handle() = default;
  Handle(const Handle& other) = default;
  ~Handle() = default;
  Handle& operator=(const Handle& other) = default;

  // The object type returned by `Get()`.
  using ObjectType = Factory::ObjectType;

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

  // Make handles usable in Abseil containers.
  template <typename H>
  friend H AbslHashValue(H hash_state,
                         const SupportsHandles<Factory>::Handle& handle) {
    return H::combine(std::move(hash_state), handle.raw_value());
  }

 private:
  int32_t raw_value_ = NullValue;
};

template <typename Factory>
  requires internal::IsHandleFactory<Factory>
typename SupportsHandles<Factory>::Handle SupportsHandles<Factory>::GetHandle()
    const {
  return Handle(handle_value());
}

template <typename Factory>
  requires internal::IsHandleFactory<Factory>
SupportsHandles<Factory>::Handle::ObjectType*
SupportsHandles<Factory>::Handle::Get() const {
  return static_cast<Factory::ObjectType*>(
      SupportsHandlesBase::LookupHandle(Factory::GetInstance(), raw_value_));
}

}  // namespace tabs

// Internal; do not use.
#define DECLARE_BASE_HANDLE_FACTORY_IMPL(ExportName, ForType) \
  class ForType;                                              \
  class ExportName ForType##HandleFactoryBase                 \
      : public ::tabs::internal::HandleFactoryT<ForType> {    \
   protected:                                                 \
    ForType##HandleFactoryBase() = default;                   \
    ~ForType##HandleFactoryBase() override = default;         \
  }

// Internal; do not use.
#define DECLARE_HANDLE_FACTORY_IMPL(ExportName, ForType)       \
  DECLARE_BASE_HANDLE_FACTORY_IMPL(ExportName, ForType);       \
  class ExportName ForType##HandleFactory final                \
      : public ForType##HandleFactoryBase {                    \
   public:                                                     \
    static ForType##HandleFactory& GetInstance();              \
                                                               \
   private:                                                    \
    friend class ::base::NoDestructor<ForType##HandleFactory>; \
    ForType##HandleFactory() = default;                        \
    ~ForType##HandleFactory() override = default;              \
  }

// Put this in your .h file just before declaring class `ForType` which inherits
// from `SupportsHandleFactory`. This will create a handle factory class named
// "`ForType`HandleFactory".
#define DECLARE_HANDLE_FACTORY(ForType) DECLARE_HANDLE_FACTORY_IMPL(, ForType)

// Use this version of `DECLARE_HANDLE_FACTORY()` if you need your classes to be
// exported, e.g.:
// ```
//  DECLARE_HANDLE_FACTORY_EXPORT(COMPONENT_EXPORT(MY_COMPONENT), MyClass)
// ```
#define DECLARE_HANDLE_FACTORY_EXPORT(ExportName, ForType) \
  DECLARE_HANDLE_FACTORY_IMPL(ExportName, ForType)

// Put this in your .h file just before declaring a class that inherits from
// `ForTypeHandleFactoryBase`.
#define DECLARE_BASE_HANDLE_FACTORY(ForType) \
  DECLARE_BASE_HANDLE_FACTORY_IMPL(, ForType)

// Put this in the .cc file that corresponds to the .h file with your
// `DECLARE_HANDLE_FACTORY()`.
#define DEFINE_HANDLE_FACTORY(ForType)                            \
  ForType##HandleFactory& ForType##HandleFactory::GetInstance() { \
    static base::NoDestructor<ForType##HandleFactory> instance;   \
    return *instance;                                             \
  }                                                               \
  /* Included to force a semicolon. */                            \
  static_assert(true)

#endif  // COMPONENTS_TABS_PUBLIC_SUPPORTS_HANDLES_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_IMMUTABLE_H_
#define COMPONENTS_SYNC_BASE_IMMUTABLE_H_

#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

// Immutable<T> provides an easy, cheap, and thread-safe way to pass
// large immutable data around.
//
// For example, consider the following code:
//
//  typedef std::vector<LargeObject> LargeObjectList;
//
//   void ProcessStuff(const LargeObjectList& stuff) {
//     for (LargeObjectList::const_iterator it = stuff.begin();
//          it != stuff.end(); ++it) {
//       ... process it ...
//     }
//   }
//
//   ...
//
//   LargeObjectList my_stuff;
//   ... fill my_stuff with lots of LargeObjects ...
//   some_loop->PostTask(FROM_HERE, base::BindOnce(&ProcessStuff, my_stuff));
//
// The last line incurs the cost of copying my_stuff, which is
// undesirable.  Here's the above code re-written using Immutable<T>:
//
//   void ProcessStuff(const Immutable<LargeObjectList>& stuff) {
//     for (LargeObjectList::const_iterator it = stuff.Get().begin();
//          it != stuff.Get().end(); ++it) {
//       ... process it ...
//     }
//   }
//
//   ...
//
//   LargeObjectList my_stuff;
//   ... fill my_stuff with lots of LargeObjects ...
//   some_loop->PostTask(
//       FROM_HERE, base::BindOnce(&ProcessStuff, MakeImmutable(&my_stuff)));
//
// The last line, which resets my_stuff to a default-initialized
// state, incurs only the cost of a swap of LargeObjectLists, which is
// O(1) for most STL container implementations.  The data in my_stuff
// is ref-counted (thread-safely), so it is freed as soon as
// ProcessStuff is finished.
//
// NOTE: By default, Immutable<T> relies on ADL
// (http://en.wikipedia.org/wiki/Argument-dependent_name_lookup) to
// find a swap() function for T, falling back to std::swap() when
// necessary.  If you overload swap() for your type in its namespace,
// Immutable<T> should be able to find it.
//
// Alternatively, you could explicitly control which swap function is
// used by providing your own traits class or using one of the
// pre-defined ones below.  See comments on traits below for details.
//
// NOTE: Some complexity is necessary in order to use Immutable<T>
// with forward-declared types.  See comments on traits below for
// details.

namespace syncer {

namespace internal {
// This class is part of the Immutable implementation.  DO NOT USE
// THIS CLASS DIRECTLY YOURSELF.

template <typename T, typename Traits>
class ImmutableCore
    : public base::RefCountedThreadSafe<ImmutableCore<T, Traits>> {
 public:
  // wrapper_ is always explicitly default-initialized to handle
  // primitive types and the case where Traits::Wrapper == T.

  ImmutableCore() : wrapper_() { Traits::InitializeWrapper(&wrapper_); }

  explicit ImmutableCore(T* t) : wrapper_() {
    Traits::InitializeWrapper(&wrapper_);
    Traits::Swap(Traits::UnwrapMutable(&wrapper_), t);
  }

  const T& Get() const { return Traits::Unwrap(wrapper_); }

 private:
  friend class base::RefCountedThreadSafe<ImmutableCore<T, Traits>>;

  ~ImmutableCore() { Traits::DestroyWrapper(&wrapper_); }

  // This is semantically const, but we can't mark it a such as we
  // modify it in the constructor.
  typename Traits::Wrapper wrapper_;

  DISALLOW_COPY_AND_ASSIGN(ImmutableCore);
};

}  // namespace internal

// Traits usage notes
// ------------------
// The most common reason to use your own traits class is to provide
// your own swap method.  First, consider the pre-defined traits
// classes HasSwapMemFn{ByRef,ByPtr} below.  If neither of those work,
// then define your own traits class inheriting from
// DefaultImmutableTraits<YourType> (to pick up the defaults for
// everything else) and provide your own Swap() method.
//
// Another reason to use your own traits class is to be able to use
// Immutable<T> with a forward-declared type (important for protobuf
// classes, when you want to avoid headers pulling in generated
// headers).  (This is why the Traits::Wrapper type exists; normally,
// Traits::Wrapper is just T itself, but that needs to be changed for
// forward-declared types.)
//
// For example, if you want to do this:
//
//   my_class.h
//   ----------
//   #include ".../immutable.h"
//
//   // Forward declaration.
//   class SomeOtherType;
//
//   class MyClass {
//     ...
//    private:
//     // Doesn't work, as defaults traits class needs SomeOtherType's
//     // definition to be visible.
//     Immutable<SomeOtherType> foo_;
//   };
//
// You'll have to do this:
//
//   my_class.h
//   ----------
//   #include ".../immutable.h"
//
//   // Forward declaration.
//   class SomeOtherType;
//
//   class MyClass {
//     ...
//    private:
//     struct ImmutableSomeOtherTypeTraits {
//       // std::unique_ptr<SomeOtherType> won't work here, either.
//       typedef SomeOtherType* Wrapper;
//
//       static void InitializeWrapper(Wrapper* wrapper);
//
//       static void DestroyWrapper(Wrapper* wrapper);
//       ...
//     };
//
//     typedef Immutable<SomeOtherType, ImmutableSomeOtherTypeTraits>
//         ImmutableSomeOtherType;
//
//     ImmutableSomeOtherType foo_;
//   };
//
//   my_class.cc
//   -----------
//   #include ".../some_other_type.h"
//
//   void MyClass::ImmutableSomeOtherTypeTraits::InitializeWrapper(
//       Wrapper* wrapper) {
//     *wrapper = new SomeOtherType();
//   }
//
//   void MyClass::ImmutableSomeOtherTypeTraits::DestroyWrapper(
//       Wrapper* wrapper) {
//     delete *wrapper;
//   }
//
//   ...
//
// Also note that this incurs an additional memory allocation when you
// create an Immutable<SomeOtherType>.

template <typename T>
struct DefaultImmutableTraits {
  using Wrapper = T;

  static void InitializeWrapper(Wrapper* wrapper) {}

  static void DestroyWrapper(Wrapper* wrapper) {}

  static const T& Unwrap(const Wrapper& wrapper) { return wrapper; }

  static T* UnwrapMutable(Wrapper* wrapper) { return wrapper; }

  static void Swap(T* t1, T* t2) {
    // Uses ADL (see
    // http://en.wikipedia.org/wiki/Argument-dependent_name_lookup).
    using std::swap;
    swap(*t1, *t2);
  }
};

// Most STL containers have by-reference swap() member functions,
// although they usually already overload std::swap() to use those.
template <typename T>
struct HasSwapMemFnByRef : public DefaultImmutableTraits<T> {
  static void Swap(T* t1, T* t2) { t1->swap(*t2); }
};

// Most Google-style objects have by-pointer Swap() member functions
// (for example, generated protocol buffer classes).
template <typename T>
struct HasSwapMemFnByPtr : public DefaultImmutableTraits<T> {
  static void Swap(T* t1, T* t2) { t1->Swap(t2); }
};

template <typename T, typename Traits = DefaultImmutableTraits<T>>
class Immutable {
 public:
  // Puts the underlying object in a default-initialized state.
  Immutable() : core_(new internal::ImmutableCore<T, Traits>()) {}

  // Copy constructor and assignment welcome.

  // Resets |t| to a default-initialized state.
  explicit Immutable(T* t) : core_(new internal::ImmutableCore<T, Traits>(t)) {}

  const T& Get() const { return core_->Get(); }

 private:
  scoped_refptr<const internal::ImmutableCore<T, Traits>> core_;
};

// Helper function to avoid having to write out template arguments.
template <typename T>
Immutable<T> MakeImmutable(T* t) {
  return Immutable<T>(t);
}

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_IMMUTABLE_H_

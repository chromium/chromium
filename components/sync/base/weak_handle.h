// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_WEAK_HANDLE_H_
#define COMPONENTS_SYNC_BASE_WEAK_HANDLE_H_

#include <cstddef>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"

// Weak handles provides a way to refer to weak pointers from another sequence.
// This is useful because it is not safe to reference a weak pointer from a
// sequence other than the sequence on which it will be invalidated.
//
// Weak handles can be passed across sequences, so for example, you can use them
// to do the "real" work on one thread and get notified on another thread:
//
// class FooIOWorker {
//  public:
//   FooIOWorker(const WeakHandle<Foo>& foo) : foo_(foo) {}
//
//   void OnIOStart() {
//     foo_.Call(FROM_HERE, &Foo::OnIOStart);
//   }
//
//  private:
//   const WeakHandle<Foo> foo_;
// };
//
// class Foo {
//  public:
//   Foo() {
//     SpawnFooIOWorkerOnIOThread(
//         MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()));
//   }
//
//   /* Will always be called on the correct sequence, and only if this
//      object hasn't been destroyed. */
//   void OnIOStart() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); .. }
//
//  private:
//   SEQUENCE_CHECKER(sequence_checker_);
//
//   base::WeakPtrFactory<Foo> weak_ptr_factory_{this};
// };

namespace base {
class Location;
}  // namespace base

namespace syncer {

template <typename T>
class WeakHandle;

namespace internal {
// These classes are part of the WeakHandle implementation.  DO NOT
// USE THESE CLASSES DIRECTLY YOURSELF.

// Base class for WeakHandleCore<T> to avoid template bloat.  Handles
// the interaction with the owner thread and its message loop.
class WeakHandleCoreBase {
 public:
  // Assumes the current thread is the owner thread.
  WeakHandleCoreBase();

  WeakHandleCoreBase(const WeakHandleCoreBase&) = delete;
  WeakHandleCoreBase& operator=(const WeakHandleCoreBase&) = delete;

  // May be called on any thread.
  bool IsOnOwnerThread() const;

 protected:
  // May be destroyed on any thread.
  ~WeakHandleCoreBase();

  // May be called on any thread.
  void PostToOwnerThread(const base::Location& from_here,
                         base::OnceClosure fn) const;

 private:
  // May be used on any thread.
  const scoped_refptr<base::SequencedTaskRunner> owner_loop_task_runner_;
};

// WeakHandleCore<T> contains all the logic for WeakHandle<T>.
template <typename T>
class WeakHandleCore : public WeakHandleCoreBase,
                       public base::RefCountedThreadSafe<WeakHandleCore<T>> {
 public:
  // Must be called on |ptr|'s owner thread, which is assumed to be
  // the current thread.
  explicit WeakHandleCore(const base::WeakPtr<T>& ptr) : ptr_(ptr) {}

  WeakHandleCore(const WeakHandleCore&) = delete;
  WeakHandleCore& operator=(const WeakHandleCore&) = delete;

  // Must be called on |ptr_|'s owner thread.
  base::WeakPtr<T> Get() const {
    DCHECK(IsOnOwnerThread());
    return ptr_;
  }

  // Call(...) may be called on any thread, but all its arguments
  // should be safe to be bound and copied across threads.
  template <typename Method, typename... Args>
  void Call(const base::Location& from_here,
            Method method,
            Args&&... args) const {
    PostToOwnerThread(
        from_here, base::BindOnce(method, ptr_, std::forward<Args>(args)...));
  }

 private:
  friend class base::RefCountedThreadSafe<WeakHandleCore<T>>;

  // May be destroyed on any thread.
  ~WeakHandleCore() = default;

  // Must be dereferenced only on the owner thread.  May be destroyed
  // from any thread.
  base::WeakPtr<T> ptr_;
};

}  // namespace internal

// May be destroyed on any thread.
// Copying and assignment are welcome.
template <typename T>
class WeakHandle {
 public:
  // Creates an uninitialized WeakHandle.
  WeakHandle() = default;

  // Creates an initialized WeakHandle from |ptr|.
  explicit WeakHandle(const base::WeakPtr<T>& ptr)
      : core_(new internal::WeakHandleCore<T>(ptr)) {}

  // Allow conversion from WeakHandle<U> to WeakHandle<T> if U is
  // convertible to T, but we *must* be on |other|'s owner thread.
  // Note that this doesn't override the regular copy constructor, so
  // that one can be called on any thread.
  template <typename U>
  WeakHandle(const WeakHandle<U>& other)  // NOLINT
      : core_(other.IsInitialized()
                  ? new internal::WeakHandleCore<T>(other.Get())
                  : nullptr) {}

  // Returns true iff this WeakHandle is initialized.  Note that being
  // initialized isn't a guarantee that the underlying object is still
  // alive.
  bool IsInitialized() const { return core_.get() != nullptr; }

  // Resets to an uninitialized WeakHandle.
  void Reset() { core_ = nullptr; }

  // Must be called only on the underlying object's owner thread.
  base::WeakPtr<T> Get() const {
    DCHECK(IsInitialized());
    DCHECK(core_->IsOnOwnerThread());
    return core_->Get();
  }

  // Call(...) may be called on any thread, but all its arguments
  // should be safe to be bound and copied across threads.
  template <typename Method, typename... Args>
  void Call(const base::Location& from_here,
            Method method,
            Args&&... args) const {
    DCHECK(IsInitialized());
    core_->Call(from_here, method, std::forward<Args>(args)...);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WeakHandleTest, TypeConversionConstructor);
  FRIEND_TEST_ALL_PREFIXES(WeakHandleTest, TypeConversionConstructorAssignment);

  scoped_refptr<internal::WeakHandleCore<T>> core_;
};

// Makes a WeakHandle from a WeakPtr.
template <typename T>
WeakHandle<T> MakeWeakHandle(const base::WeakPtr<T>& ptr) {
  return WeakHandle<T>(ptr);
}

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_WEAK_HANDLE_H_

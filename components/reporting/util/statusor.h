// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StatusOr<T> is the union of a Status object and a T
// object. StatusOr models the concept of an object that is either a
// usable value, or an error Status explaining why such a value is
// not present. To this end, StatusOr<T> does not allow its Status
// value to be Status::StatusOK(). Further, StatusOr<T*> does not allow the
// contained pointer to be nullptr.
//
// The primary use-case for StatusOr<T> is as the return value of a
// function which may fail.
//
// Example client usage for a StatusOr<T>, where T is not a pointer:
//
//  StatusOr<float> result = DoBigCalculationThatCouldFail();
//  if (result.ok()) {
//    float answer = result.ValueOrDie();
//    printf("Big calculation yielded: %f", answer);
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<T*>:
//
//  StatusOr<Foo*> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo(result.ValueOrDie());
//    foo->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<std::unique_ptr<T>>:
//
//  StatusOr<std::unique_ptr<Foo>> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo = std::move(result.ValueOrDie());
//    foo->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example factory implementation returning StatusOr<T*>:
//
//  StatusOr<Foo*> FooFactory::MakeNewFoo(int arg) {
//    if (arg <= 0) {
//      return Status(error::INVALID_ARGUMENT, "Arg must be positive");
//    } else {
//      return new Foo(arg);
//    }
//  }
//

#ifndef COMPONENTS_REPORTING_UTIL_STATUSOR_H_
#define COMPONENTS_REPORTING_UTIL_STATUSOR_H_

#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "components/reporting/util/status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

namespace internal {

// Helper class for StatusOr to use.
class StatusOrHelper {
 public:
  static const Status& NotInitializedStatus();
  static const Status& MovedOutStatus();
  static void Crash(const Status& status);
};

}  // namespace internal

template <typename T>
class [[nodiscard]] StatusOr {
  template <typename U>
  friend class StatusOr;

  // A traits class that determines whether a type U is implicitly convertible
  // from a type V. If it is convertible, then the |value| member of this class
  // is statically set to true, otherwise it is statically set to false.
  template <class U, typename V>
  struct is_implicitly_constructible
      : std::conjunction<std::is_constructible<U, V>,
                         std::is_convertible<V, U>> {};

 public:
  // Constructs a new StatusOr with UNINITIALIZED status and no value.
  StatusOr() : status_(internal::StatusOrHelper::NotInitializedStatus()) {}

  // Constructs a new StatusOr with the given non-ok status. After calling
  // this constructor, calls to ValueOrDie() will CHECK-fail.
  //
  // This constructor is not declared explicit so that a function with a return
  // type of |StatusOr<T>| can return a Status object, and the status will be
  // implicitly converted to the appropriate return type as a matter of
  // convenience.
  // REQUIRES: !status.ok().
  StatusOr(const Status& status)  // NOLINT(runtime/explicit)
      : status_(status) {
    if (status.ok()) {
      internal::StatusOrHelper::Crash(status);
    }
  }

  // Constructs a StatusOr object that contains |value|. The resulting object
  // is considered to have an OK status. The wrapped element can be accessed
  // with ValueOrDie().
  //
  // This constructor is made implicit so that a function with a return type of
  // |StatusOr<T>| can return an object of type |U&&|, implicitly converting
  // it to a |StatusOr<T>| object.
  //
  // Note that |T| must be implicitly constructible from |U|, and |U| must not
  // be a (cv-qualified) Status or Status-reference type. Due to C++
  // reference-collapsing rules and perfect-forwarding semantics, this
  // constructor matches invocations that pass |value| either as a const
  // reference or as an rvalue reference. Since StatusOr needs to work for both
  // reference and rvalue-reference types, the constructor uses perfect
  // forwarding to avoid invalidating arguments that were passed by reference.
  template <typename U,
            typename E = typename std::enable_if<
                is_implicitly_constructible<T, U>::value &&
                !std::is_same<typename std::remove_reference<
                                  typename std::remove_cv<U>::type>::type,
                              Status>::value>::type>
  StatusOr(U&& value)  // NOLINT(runtime/explicit)
      : status_(Status::StatusOK()), value_(std::forward<U>(value)) {}

  // Copy constructor.
  //
  // This constructor needs to be explicitly defined because the presence of
  // the move-assignment operator deletes the default copy constructor. In such
  // a scenario, since the deleted copy constructor has stricter binding rules
  // than the templated copy constructor, the templated constructor cannot act
  // as a copy constructor, and any attempt to copy-construct a |StatusOr|
  // object results in a compilation error.
  StatusOr(const StatusOr& other)
      : status_(other.status_), value_(other.value_) {}

  // Templatized constructor that constructs a |StatusOr<T>| from a const
  // reference to a |StatusOr<U>|.
  //
  // |T| must be implicitly constructible from |const U&|.
  template <typename U,
            typename E = typename std::enable_if<
                is_implicitly_constructible<T, const U&>::value>::type>
  StatusOr(const StatusOr<U>& other)  // NOLINT(runtime/explicit)
      : status_(other.status_), value_(other.value_) {}

  // Copy-assignment operator.
  StatusOr& operator=(const StatusOr& other) {
    // Check for self-assignment.
    if (this == &other) {
      return *this;
    }

    if (other.status_.ok()) {
      AssignValue(other.value_);
    } else {
      AssignStatus(other.status_);
    }
    return *this;
  }

  // Templatized constructor which constructs a |StatusOr<T>| by moving the
  // contents of a |StatusOr<U>|. |T| must be implicitly constructible from
  // |U&&|.
  //
  // Sets |other| to contain a non-OK status with a|error::UNKNOWN|
  // error code.
  template <typename U,
            typename E = typename std::enable_if<
                is_implicitly_constructible<T, U&&>::value>::type>
  StatusOr(StatusOr<U>&& other)  // NOLINT(runtime/explicit)
      : status_(std::move(other.status_)), value_(std::move(other.value_)) {
    other.status_ = internal::StatusOrHelper::MovedOutStatus();
  }

  // Move-assignment operator.
  //
  // Sets |other| to contain a non-OK status with a |error::UNKNOWN| error
  // code.
  StatusOr& operator=(StatusOr&& other) {
    // Check for self-assignment.
    if (this == &other) {
      return *this;
    }

    if (other.status_.ok()) {
      AssignValue(std::move(other.value_.value()));
    } else {
      AssignStatus(std::move(other.status_));
    }
    other.status_ = internal::StatusOrHelper::MovedOutStatus();

    return *this;
  }

  // Indicates whether the object contains a |T| value.
  bool ok() const { return status_.ok(); }

  // Gets the stored status object, or an OK status if a |T| value is stored.
  Status status() const { return status_; }

  // Gets the stored |T| value.
  //
  // This method should only be called if this StatusOr object's status is OK
  // (i.e. a call to ok() returns true), otherwise this call will abort.
  [[nodiscard]] const T& ValueOrDie() const& {
    if (!ok()) {
      internal::StatusOrHelper::Crash(status_);
    }
    return value_.value();
  }

  // Gets a mutable reference to the stored |T| value.
  //
  // This method should only be called if this StatusOr object's status is OK
  // (i.e. a call to ok() returns true), otherwise this call will abort.
  [[nodiscard]] T& ValueOrDie() & {
    if (!ok()) {
      internal::StatusOrHelper::Crash(status_);
    }
    return value_.value();
  }

  // Moves and returns the internally-stored |T| value.
  //
  // This method should only be called if this StatusOr object's status is OK
  // (i.e. a call to ok() returns true), otherwise this call will abort. The
  // StatusOr object is invalidated after this call and will be updated to
  // contain a non-OK status with a |error::UNKNOWN| error code.
  [[nodiscard]] T ValueOrDie() && {
    if (!ok()) {
      internal::StatusOrHelper::Crash(status_);
    }

    // Invalidate this StatusOr object before returning control to caller.
    StatusResetter set_moved_status(this,
                                    internal::StatusOrHelper::MovedOutStatus());
    return std::move(value_.value());
  }

 private:
  class StatusResetter {
   public:
    StatusResetter(StatusOr<T>* status_or, const Status& reset_to_status)
        : status_or_(status_or), reset_to_status_(reset_to_status) {}
    StatusResetter(const StatusResetter& other) = delete;
    StatusResetter& operator=(const StatusResetter& other) = delete;
    ~StatusResetter() {
      status_or_->OverwriteValueWithStatus(reset_to_status_);
    }

   private:
    const raw_ptr<StatusOr<T>> status_or_;
    const Status reset_to_status_;
  };

  // Resets |this| to contain |status|.
  template <class U>
  void AssignStatus(U&& status) {
    if (ok()) {
      OverwriteValueWithStatus(std::forward<U>(status));
    } else {
      status_ = std::forward<U>(status);
    }
  }

  // Under the assumption that |this| is currently holding a value, resets the
  // |value_| member and sets |status_| to indicate that |this| does not have
  // a value.
  template <class U>
  void OverwriteValueWithStatus(U&& status) {
    if (!ok()) {
      LOG(FATAL) << "Object does not have a value to change from";
    }
    value_.reset();
    status_ = std::forward<U>(status);
  }

  // Resets |value_| to contain the |value| and sets |status_|
  // to OK, indicating that the StatusOr object has a value.
  // Destroys the existing |value_|.
  template <class U>
  void AssignValue(U&& value) {
    value_ = std::forward<U>(value);
    status_ = Status::StatusOK();
  }

  Status status_;
  absl::optional<T> value_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_STATUSOR_H_

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
//    float answer = result.value();
//    printf("Big calculation yielded: %f", answer);
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<T*>:
//
//  StatusOr<Foo*> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo(result.value());
//    foo->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<std::unique_ptr<T>>:
//
//  StatusOr<std::unique_ptr<Foo>> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo = std::move(result.value());
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
#include "base/types/expected.h"
#include "components/reporting/util/status.h"

namespace reporting {

namespace internal {

// Helper class for StatusOr to use.
class StatusOrHelper {
 public:
  static const base::unexpected<Status>& NotInitializedStatus();
  static const base::unexpected<Status>& MovedOutStatus();
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
  StatusOr() : expected_(internal::StatusOrHelper::NotInitializedStatus()) {}

  // Constructs a new StatusOr with the given non-ok status. After calling
  // this constructor, calls to value() will CHECK-fail.
  //
  // This constructor is not declared explicit so that a function with a return
  // type of |StatusOr<T>| can return a Status object, and the status will be
  // implicitly converted to the appropriate return type as a matter of
  // convenience.
  // REQUIRES: !status.ok().
  StatusOr(const Status& status)  // NOLINT(google-explicit-constructor)
      : expected_(base::unexpected<Status>(status)) {
    if (status.ok()) {
      internal::StatusOrHelper::Crash(status);
    }
  }

  // Constructs a StatusOr object that contains |value|. The resulting object
  // is considered to have an OK status. The wrapped element can be accessed
  // with value().
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
  StatusOr(U&& value)  // NOLINT(google-explicit-constructor)
      : expected_(std::forward<U>(value)) {}

  // Copy constructor.
  //
  // This constructor needs to be explicitly defined because the presence of
  // the move-assignment operator deletes the default copy constructor. In such
  // a scenario, since the deleted copy constructor has stricter binding rules
  // than the templated copy constructor, the templated constructor cannot act
  // as a copy constructor, and any attempt to copy-construct a |StatusOr|
  // object results in a compilation error.
  StatusOr(const StatusOr& other) : expected_(other.expected_) {}

  // Templatized constructor that constructs a |StatusOr<T>| from a const
  // reference to a |StatusOr<U>|.
  //
  // |T| must be implicitly constructible from |const U&|.
  template <typename U,
            typename E = typename std::enable_if<
                is_implicitly_constructible<T, const U&>::value>::type>
  StatusOr(const StatusOr<U>& other)  // NOLINT(google-explicit-constructor)
      : expected_(other.expected_) {}

  // Copy-assignment operator.
  StatusOr& operator=(const StatusOr& other) {
    // Check for self-assignment.
    if (this == &other) {
      return *this;
    }

    expected_ = other.expected_;
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
  StatusOr(StatusOr<U>&& other)  // NOLINT(google-explicit-constructor)
      : expected_(std::move(other.expected_)) {
    other.expected_ = internal::StatusOrHelper::MovedOutStatus();
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

    this->expected_ = std::move(other.expected_);
    other.expected_ = internal::StatusOrHelper::MovedOutStatus();

    return *this;
  }

  // Indicates whether the object contains a |T| value.
  bool ok() const { return expected_.has_value(); }

  // Gets the stored status object, or an OK status if a |T| value is stored.
  Status status() const {
    return (expected_.has_value() ? Status::StatusOK() : expected_.error());
  }

  // Proxies of `base::expected<T, Status>::value`. They would crash if it does
  // not have a value.
  [[nodiscard]] const T& value() const& { return expected_.value(); }
  [[nodiscard]] T& value() & { return expected_.value(); }
  [[nodiscard]] T value() && { return std::move(expected_).value(); }

 private:
  base::expected<T, Status> expected_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_STATUSOR_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_STATUS_MATCHERS_H_
#define CHROMECAST_CAST_CORE_GRPC_STATUS_MATCHERS_H_

#include <string>
#include <type_traits>

#include "testing/gmock/include/gmock/gmock.h"

namespace cast {
namespace test {
namespace internal {

// Checks if a type T has a status() API helping distinguish if Status impl is
// nested inside T (eg GrpcStatusOr etc) or T is the Status impl.
template <typename T>
class has_status_api {
  struct no {};

  template <typename C>
  static decltype(std::declval<C>().status()) test(int);
  template <typename C>
  static no test(...);

 public:
  enum { value = !std::is_same<decltype(test<T>(0)), no>::value };
};

// Checks if a Status impl has 'code' API deducing that status code and message
// can be retrieved using T::code() and T::message APIs (eg absl::Status).
template <typename T>
class has_code_api {
  struct no {};

  template <typename C>
  static decltype(&C::code) test(int);
  template <typename C>
  static no test(...);

 public:
  enum { value = !std::is_same<decltype(test<T>(0)), no>::value };
};

// Checks if a Status impl has 'error_code' API deducing that status code and
// message can be retrieved using T::error_code() and T::error_message APIs (eg
// grpc::Status).
template <typename T>
class has_error_code_api {
  struct no {};

  template <typename C>
  static decltype(&C::error_code) test(int);
  template <typename C>
  static no test(...);

 public:
  enum { value = !std::is_same<decltype(test<T>(0)), no>::value };
};

// Reads the error code and message of the generic TStatus which can be either a
// wrapper object with TStatus::status() defined or the actual status object
// with TStatus::code\message() or TStatus::error_code\error_message APIs. The
// resolution works based on SFINAE when invalid type substitution results in
// dropping the method from compilation.
class StatusResolver {
 public:
  struct StatusInfo {
    int code;
    std::string message;
  };

  // Constructor that deduces the actual APIs of TStatus and sets the code and
  // message.
  template <typename TStatus>
  explicit StatusResolver(const TStatus& status)
      : status_info_(ResolveStatusInfo(ResolveStatus(status))) {}
  ~StatusResolver();

  // Returns the status code (as an integer) and message.
  const StatusInfo& status_info() const { return status_info_; }

  // Checks if status is ok.
  bool ok() const { return status_info_.code == 0; }

 private:
  // Returns the actual status from the wrapper.
  template <typename TWrappedStatus,
            typename std::enable_if<has_status_api<TWrappedStatus>::value,
                                    TWrappedStatus>::type* = nullptr>
  static auto ResolveStatus(const TWrappedStatus& wrapped_status)
      -> decltype(std::declval<const TWrappedStatus>().status()) {
    return wrapped_status.status();
  }

  // Returns itself.
  template <typename TStatus,
            typename std::enable_if<!has_status_api<TStatus>::value,
                                    TStatus>::type* = nullptr>
  static auto ResolveStatus(const TStatus& status) -> TStatus {
    return status;
  }

  // Returns the pair of integer code and error message using TStatus::code()
  // and TStatus::message() APIs.
  template <typename TStatus,
            typename std::enable_if<has_code_api<TStatus>::value,
                                    TStatus>::type* = nullptr>
  static StatusInfo ResolveStatusInfo(const TStatus& status) {
    return StatusInfo(
        {static_cast<int>(status.code()), std::string(status.message())});
  }

  // Returns the pair of integer code and error message using
  // TStatus::error_code() and TStatus::error_message() APIs.
  template <typename TStatus,
            typename std::enable_if<has_error_code_api<TStatus>::value,
                                    TStatus>::type* = nullptr>
  static StatusInfo ResolveStatusInfo(const TStatus& status) {
    return StatusInfo({static_cast<int>(status.error_code()),
                       std::string(status.error_message())});
  }

  const StatusInfo status_info_;
};

// Implementation of MatcherInterface for StatusIs matcher.
template <typename TStatus>
class StatusIsImpl : public ::testing::MatcherInterface<TStatus> {
 public:
  StatusIsImpl(::testing::Matcher<int> code,
               ::testing::Matcher<std::string> message)
      : code_(std::move(code)), message_(std::move(message)) {}

  // MatcherInterface implementation.
  void DescribeTo(std::ostream* os) const override {
    *os << "has code() that ";
    code_.DescribeTo(os);
    *os << " and message() that ";
    message_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "has code() that ";
    code_.DescribeNegationTo(os);
    *os << " and message() that ";
    message_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(
      TStatus status,
      ::testing::MatchResultListener* result_listener) const override {
    ::testing::StringMatchResultListener listener;
    StatusResolver status_resolver(status);
    auto status_info = status_resolver.status_info();
    if (!code_.Matches(status_info.code)) {
      *result_listener << "has wrong status code " << status_info.code;
      return false;
    }

    if (!message_.Matches(status_info.message)) {
      *result_listener << "has wrong error message " << status_info.message;
      return false;
    }

    return true;
  }

 private:
  const ::testing::Matcher<int> code_;
  const ::testing::Matcher<std::string> message_;
};

// Allows usage of StatusIs without template parameter
class StatusIsPolymorphicWrapper {
 public:
  template <typename TStatusCode>
  StatusIsPolymorphicWrapper(TStatusCode code,
                             ::testing::Matcher<std::string>&& message)
      : code_(static_cast<int>(code)), message_(std::move(message)) {}
  StatusIsPolymorphicWrapper(const StatusIsPolymorphicWrapper&);
  ~StatusIsPolymorphicWrapper();

  // Converts this polymorphic matcher to a monomorphic matcher.
  template <typename TStatus>
  operator ::testing::Matcher<TStatus>() const {
    return ::testing::Matcher<TStatus>(
        new StatusIsImpl<TStatus>(std::move(code_), std::move(message_)));
  }

 private:
  ::testing::Matcher<int> code_;
  ::testing::Matcher<std::string> message_;
};

// Allows usage of IsOk without template parameter
template <typename TStatus>
class IsOkImpl : public ::testing::MatcherInterface<TStatus> {
 public:
  void DescribeTo(std::ostream* os) const override {
    *os << "is OK";
  }  // namespace internal
  void DescribeNegationTo(std::ostream* os) const override {
    *os << "is not OK";
  }
  bool MatchAndExplain(TStatus actual_value,
                       ::testing::MatchResultListener*) const override {
    return StatusResolver(actual_value).ok();
  }
};  // namespace test

class IsOkPolymorphicWrapper {
 public:
  template <typename TStatus>
  operator ::testing::Matcher<TStatus>() const {  // NOLINT
    return ::testing::Matcher<TStatus>(new IsOkImpl<const TStatus&>());
  }
};

}  // namespace internal

template <typename TStatusCode>
inline internal::StatusIsPolymorphicWrapper StatusIs(TStatusCode code) {
  return internal::StatusIsPolymorphicWrapper(code, ::testing::_);
}

template <typename TStatusCode>
inline internal::StatusIsPolymorphicWrapper StatusIs(
    TStatusCode code,
    ::testing::Matcher<std::string>&& message_matcher) {
  return internal::StatusIsPolymorphicWrapper(code, std::move(message_matcher));
}

inline internal::IsOkPolymorphicWrapper IsOk() {
  return internal::IsOkPolymorphicWrapper();
}

#define CU_EXPECT_OK(expression) EXPECT_THAT(expression, ::cast::test::IsOk())
#define CU_ASSERT_OK(expression) ASSERT_THAT(expression, ::cast::test::IsOk())
#define CU_CHECK_OK(expression) \
  CHECK(::cast::test::internal::StatusResolver(expression).ok())

}  // namespace test
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_STATUS_MATCHERS_H_

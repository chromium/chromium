// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_STATUS_OR_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_STATUS_OR_H_

#include <grpcpp/support/status.h>

#include <optional>
#include <string>
#include <type_traits>

#include "base/check.h"

namespace cast {
namespace utils {

// Converts grpc::Status to a string message.
std::string GrpcStatusToString(const grpc::Status& status);

// Holds a value of type T with grpc::Status::OK or an error grpc::Status.
template <typename T,
          typename std::enable_if<!std::is_reference<T>::value, T*>::type =
              nullptr>
class GrpcStatusOr {
 public:
  GrpcStatusOr() : status_(grpc::StatusCode::UNKNOWN, "") {}

  // Constructs GrpcStatusOr from an error |status_code|.
  GrpcStatusOr(grpc::StatusCode status_code)  // NOLINT
      : GrpcStatusOr(status_code, "") {}

  // Constructs GrpcStatusOr from an error |status_code| and |error_message|.
  GrpcStatusOr(grpc::StatusCode status_code, const std::string& error_message)
      : status_(status_code, error_message) {
    DCHECK(!status_.ok());
  }

  // Constructs GrpcStatusOr from an error |status|.
  GrpcStatusOr(grpc::Status status)  // NOLINT
      : status_(std::move(status)) {
    DCHECK(!status_.ok());
  }

  // Constructs GrpcStatusOr from the |data|. Status code is set to OK.
  GrpcStatusOr(const T& data)  // NOLINT
      : status_(grpc::Status::OK), data_(data) {}

  // Constructs GrpcStatusOr from the |data|. Status code is set to OK.
  GrpcStatusOr(T&& data)  // NOLINT
      : status_(grpc::Status::OK), data_(std::move(data)) {}

  // Constructs GrpcStatusOr from the |data| of type U that is possible to
  // convert to type |T|. For example, std::unique_ptr<MockClass> converted to
  // std::unique_ptr<Class>. Status code is set to OK.
  template <typename U,
            typename std::enable_if<std::is_constructible<T, U>::value,
                                    U>::type* = nullptr>
  GrpcStatusOr(U&& data)  // NOLINT
      : status_(grpc::Status::OK), data_(std::forward<U>(data)) {}

  GrpcStatusOr(GrpcStatusOr&&) = default;
  GrpcStatusOr& operator=(GrpcStatusOr&&) = default;
  GrpcStatusOr(const GrpcStatusOr&) = default;
  GrpcStatusOr& operator=(const GrpcStatusOr&) = default;
  ~GrpcStatusOr() = default;

  // Returns if status is OK.
  bool ok() const { return status_.ok(); }

  // Returns const reference to gRPC status.
  const grpc::Status& status() const& { return status_; }

  // Returns r-value to gRPC status.
  grpc::Status&& status() && { return std::move(status_); }

  // Returns the pointer to the stored data. Checks if status is not OK.
  T* operator->() {
    DCHECK(status_.ok());
    return &*data_;
  }

  // Returns the const pointer to the stored data. Checks if status is not OK.
  const T* operator->() const {
    DCHECK(status_.ok());
    return &*data_;
  }

  // Returns the const reference to the stored data. Checks if status is not OK.
  const T& operator*() const& {
    DCHECK(status_.ok());
    return *data_;
  }

  // Returns the lvalue-ref to the underlying value. Checks if status is not OK.
  const T& value() const& {
    DCHECK(status_.ok());
    return *data_;
  }

  // Returns the rvalue-ref to the underlying value. To trigger this accessor,
  // the status object needs to be moved first: std::move(status_).value().
  // Checks if status is not OK.
  T&& value() && {
    DCHECK(status_.ok());
    status_ = grpc::Status(grpc::StatusCode::UNKNOWN, "");
    return std::move(data_).value();
  }

  // Sets the data.
  void emplace(T&& data) {
    status_ = grpc::Status::OK;
    data_.emplace(std::move(data));
  }

  std::string ToString() const { return GrpcStatusToString(status()); }

 private:
  grpc::Status status_;
  std::optional<T> data_;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_STATUS_OR_H_

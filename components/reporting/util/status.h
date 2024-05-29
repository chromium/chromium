// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_STATUS_H_
#define COMPONENTS_REPORTING_UTIL_STATUS_H_

#include <cstdint>
#include <iosfwd>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "components/reporting/proto/synced/status.pb.h"

namespace reporting {
namespace error {
// These values must match error codes defined in google/rpc/code.proto
// This also must match the order EnterpriseCloudReportingStatusCode at
// tools/metrics/histograms/enums.xml and the integer of option shouldn't be
// changed.
// If two assumptions above conflict, please create a new enum for metrics
// purposes and keep the original order.
enum Code : int32_t {
  OK = 0,
  CANCELLED = 1,
  UNKNOWN = 2,
  INVALID_ARGUMENT = 3,
  DEADLINE_EXCEEDED = 4,
  NOT_FOUND = 5,
  ALREADY_EXISTS = 6,
  PERMISSION_DENIED = 7,
  RESOURCE_EXHAUSTED = 8,
  FAILED_PRECONDITION = 9,
  ABORTED = 10,
  OUT_OF_RANGE = 11,
  UNIMPLEMENTED = 12,
  INTERNAL = 13,
  UNAVAILABLE = 14,
  DATA_LOSS = 15,
  UNAUTHENTICATED = 16,
  // The value should always be kept last.
  MAX_VALUE
};
}  // namespace error

class [[nodiscard]] Status {
 public:
  // Creates a "successful" status.
  Status();

  Status(const Status&);
  Status& operator=(const Status&);
  Status(Status&&);
  Status& operator=(Status&&);
  virtual ~Status();

  // Create a status in the canonical error space with the specified code, and
  // error message. If "code == 0", error_message is ignored and a Status object
  // identical to Status::OK is constructed.
  //
  // If a string literal is passed in, it will be copied to construct a
  // `std::string` object. While it is possible to create a constructor tag or
  // factory function to construct a `Status` object from a string literal
  // that does not copy, we think that its cost to code maintainability
  // outweighs the performance benefit it brings.
  Status(error::Code error_code, std::string error_message);

  // Pre-defined Status object
  static const Status& StatusOK();

  // Accessor
  bool ok() const { return error_code_ == error::OK; }
  int error_code() const { return error_code_; }
  error::Code code() const { return error_code_; }
  const std::string& error_message() const { return error_message_; }
  const std::string& message() const { return error_message_; }

  bool operator==(const Status& x) const;
  bool operator!=(const Status& x) const { return !operator==(x); }

  // Return a combination of the error code name and message.
  std::string ToString() const;

  // Exports the contents of this object into |status_proto|. This method sets
  // all fields in |status_proto| (for OK status clears |error_message|).
  void SaveTo(StatusProto* status_proto) const;

  // Populates this object using the contents of the given |status_proto|.
  void RestoreFrom(const StatusProto& status_proto);

 private:
  error::Code error_code_;
  std::string error_message_;
};

// Prints a human-readable representation of 'x' to 'os'.
std::ostream& operator<<(std::ostream& os, const Status& x);

// Auto runner wrapping a provided callback.
// When it goes out of scope and the callback has not been run, it is invoked
// with the `failed` value. Intended to be used in code like:
//
//   class Handler {
//    public:
//     using ResultCb = base::OnceCallback<void(Status)>;
//     Handler(...) {...}
//     void Method(..., Scoped<ResultCb> done) {
//       ...
//       std::move(done).Run(Status::StatusOK());
//     }
//   };
//   auto handler = std::make_unique<Handler>(...);
//   task_runner->PostTask(
//       FROM_HERE,
//       base::BindOnce(&Handler::Method, handler->GetWeakPtr(), ...,
//                      Scoped<ResultCb>(
//                          base::BindOnce(&Done, ...),
//                          Status(error::UNAVAILABLE,
//                                 "Handler has been destructed"))));
//
// If at run time `handler` is destructed before `Handler::Method` is executed,
// `Done` will still be called with:
//     Status(error::UNAVAILABLE, "Handler has been destructed")
//  as a result.
//
// If `Done` expects something else than `Status`, `Scoped` needs to be
// tagged with respective type - e.g.
//                      Scoped<ResultCb, StatusOr<Result>>(
//                          base::BindOnce(&Done, ...),
//                          base::unexpected(Status(error::UNAVAILABLE,
//                                           "Handler has been destructed")))
//

template <typename Failed = Status>
class Scoped : public base::OnceCallback<void(Failed)> {
 public:
  using Callback = base::OnceCallback<void(Failed)>;

  Scoped(Callback cb, Failed failed)
      : Callback(std::forward<Callback>(cb)), failed_(std::move(failed)) {}

  Scoped(Scoped&& other)
      : Callback(std::exchange<Callback>(other, base::NullCallback())),
        failed_(std::move(other.failed_)) {}

  Scoped& operator=(Scoped&& other) { return Scoped(other); }

  ~Scoped() {
    if (!Callback::is_null()) {
      std::move(*this).Run(std::move(failed_));
    }
  }

 private:
  Failed failed_;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_STATUS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_STATUS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_STATUS_H_

#include "base/types/strong_alias.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace supervised_user {

// Holds the status of ProtoFetcher fetch operation, providing additional
// context on error state.
class ProtoFetcherStatus {
 public:
  using HttpStatusOrNetErrorType =
      base::StrongAlias<class HttpStatusOrNetErrorTag, int>;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(State)
  enum class State : int {
    OK = 0,
    GOOGLE_SERVICE_AUTH_ERROR = 1,
    HTTP_STATUS_OR_NET_ERROR = 2,
    INVALID_RESPONSE = 3,
    DATA_ERROR = 4,  //  Not signalled by this fetcher itself, but might be used
                     //  by consumers to indicate data problem.
    kMaxValue = DATA_ERROR,  // Keep last, required for metrics.
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:SupervisedUserProtoFetcherStatus)

  // Status might be used in base::expected context as possible error, since it
  // contains two error-enabled attributes which are copyable / assignable.
  ProtoFetcherStatus(const ProtoFetcherStatus&);
  ProtoFetcherStatus& operator=(const ProtoFetcherStatus&);

  ~ProtoFetcherStatus();
  ProtoFetcherStatus() = delete;

  // Convenience creators instead of exposing ProtoFetcherStatus(State state).
  static ProtoFetcherStatus Ok();
  static ProtoFetcherStatus GoogleServiceAuthError(
      class GoogleServiceAuthError
          error);  // The copy follows the interface of
                   // https://source.chromium.org/chromium/chromium/src/+/main:components/signin/public/identity_manager/primary_account_access_token_fetcher.h;l=241;drc=8ba1bad80dc22235693a0dd41fe55c0fd2dbdabd
  static ProtoFetcherStatus HttpStatusOrNetError(
      int value = 0);  // Either net::Error (negative numbers, 0 denotes
                       // success) or HTTP status.
  static ProtoFetcherStatus InvalidResponse();

  // ProtoFetcherStatus::IsOk iff google_service_auth_error_.state() ==
  // NONE and state_ == NONE
  bool IsOk() const;
  // Indicates whether the status is not ok, but is worth retrying because it
  // might go away.
  bool IsTransientError() const;
  // Indicates whether the status is not ok and there is no point in retrying.
  bool IsPersistentError() const;

  // Returns a message describing the status.
  std::string ToString() const;

  State state() const;
  HttpStatusOrNetErrorType http_status_or_net_error() const;
  class GoogleServiceAuthError google_service_auth_error() const;

 private:
  // Disallows impossible states.
  explicit ProtoFetcherStatus(State state);
  explicit ProtoFetcherStatus(
      HttpStatusOrNetErrorType http_status_or_net_error);
  explicit ProtoFetcherStatus(
      class GoogleServiceAuthError
          google_service_auth_error);  // Implies State ==
                                       // GOOGLE_SERVICE_AUTH_ERROR
  ProtoFetcherStatus(State state,
                     class GoogleServiceAuthError google_service_auth_error);

  State state_;
  HttpStatusOrNetErrorType http_status_or_net_error_{
      0};  // Meaningful iff state_ == HTTP_STATUS_OR_NET_ERROR
  class GoogleServiceAuthError google_service_auth_error_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_STATUS_H_

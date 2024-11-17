// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/proto_fetcher_status.h"

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_status_code.h"

namespace supervised_user {

ProtoFetcherStatus::ProtoFetcherStatus(
    State state,
    class GoogleServiceAuthError google_service_auth_error)
    : state_(state), google_service_auth_error_(google_service_auth_error) {}

ProtoFetcherStatus::~ProtoFetcherStatus() = default;

ProtoFetcherStatus::ProtoFetcherStatus(State state) : state_(state) {
  DCHECK_NE(state, State::GOOGLE_SERVICE_AUTH_ERROR);
}

ProtoFetcherStatus::ProtoFetcherStatus(
    HttpStatusOrNetErrorType http_status_or_net_error)
    : state_(State::HTTP_STATUS_OR_NET_ERROR),
      http_status_or_net_error_(http_status_or_net_error) {}

ProtoFetcherStatus::ProtoFetcherStatus(
    class GoogleServiceAuthError google_service_auth_error)
    : ProtoFetcherStatus(State::GOOGLE_SERVICE_AUTH_ERROR,
                         google_service_auth_error) {}

ProtoFetcherStatus::ProtoFetcherStatus(const ProtoFetcherStatus& other) =
    default;

ProtoFetcherStatus& ProtoFetcherStatus::operator=(
    const ProtoFetcherStatus& other) = default;

ProtoFetcherStatus ProtoFetcherStatus::Ok() {
  return ProtoFetcherStatus(State::OK);
}

ProtoFetcherStatus ProtoFetcherStatus::GoogleServiceAuthError(
    class GoogleServiceAuthError error) {
  return ProtoFetcherStatus(error);
}

ProtoFetcherStatus ProtoFetcherStatus::HttpStatusOrNetError(
    int http_status_or_net_error) {
  return ProtoFetcherStatus(HttpStatusOrNetErrorType(http_status_or_net_error));
}

ProtoFetcherStatus ProtoFetcherStatus::InvalidResponse() {
  return ProtoFetcherStatus(State::INVALID_RESPONSE);
}

bool ProtoFetcherStatus::IsOk() const {
  return state_ == State::OK;
}

bool ProtoFetcherStatus::IsTransientError() const {
  if (state_ == State::HTTP_STATUS_OR_NET_ERROR) {
    // Ideally we should treat a wider set of HTTP status codes as permanent
    // errors (eg. most 4xx responses), but there is no standard utility in
    // Chromium to classify these and there's no harm in retrying them.
    //
    // 401 must be treated as permanent, as it has specific retry handling in
    // ProtoFetcher.
    if (http_status_or_net_error_.value() == net::HTTP_UNAUTHORIZED) {
      return false;
    }
    return true;
  }
  if (state_ == State::GOOGLE_SERVICE_AUTH_ERROR) {
    return google_service_auth_error_.IsTransientError();
  }
  return false;
}

bool ProtoFetcherStatus::IsPersistentError() const {
  if (state_ == State::INVALID_RESPONSE) {
    return true;
  }
  if (state_ == State::DATA_ERROR) {
    return true;
  }
  if (state_ == State::GOOGLE_SERVICE_AUTH_ERROR) {
    return google_service_auth_error_.IsPersistentError();
  }
  if ((state_ == State::HTTP_STATUS_OR_NET_ERROR) &&
      (http_status_or_net_error_.value() == net::HTTP_UNAUTHORIZED)) {
    return true;
  }
  return false;
}

std::string ProtoFetcherStatus::ToString() const {
  switch (state_) {
    case State::OK:
      return "ProtoFetcherStatus::OK";
    case State::GOOGLE_SERVICE_AUTH_ERROR:
      return base::StrCat({"ProtoFetcherStatus::GOOGLE_SERVICE_AUTH_ERROR: ",
                           google_service_auth_error_.ToString()});
    case State::HTTP_STATUS_OR_NET_ERROR:
      return base::StringPrintf(
          "ProtoFetcherStatus::HTTP_STATUS_OR_NET_ERROR: %d",
          http_status_or_net_error_.value());
    case State::INVALID_RESPONSE:
      return "ProtoFetcherStatus::INVALID_RESPONSE";
    case State::DATA_ERROR:
      return "ProtoFetcherStatus::DATA_ERROR";
  }
}

ProtoFetcherStatus::State ProtoFetcherStatus::state() const {
  return state_;
}

ProtoFetcherStatus::HttpStatusOrNetErrorType
ProtoFetcherStatus::http_status_or_net_error() const {
  return http_status_or_net_error_;
}

class GoogleServiceAuthError ProtoFetcherStatus::google_service_auth_error()
    const {
  return google_service_auth_error_;
}

}  // namespace supervised_user

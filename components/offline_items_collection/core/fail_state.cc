// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/fail_state.h"

namespace offline_items_collection {

bool ToFailState(int value, FailState* fail_state) {
  switch (static_cast<FailState>(value)) {
    case FailState::NO_FAILURE:
    case FailState::CANNOT_DOWNLOAD:
    case FailState::NETWORK_INSTABILITY:
    case FailState::FILE_FAILED:
    case FailState::FILE_ACCESS_DENIED:
    case FailState::FILE_NO_SPACE:
    case FailState::FILE_NAME_TOO_LONG:
    case FailState::FILE_TOO_LARGE:
    case FailState::FILE_VIRUS_INFECTED:
    case FailState::FILE_TRANSIENT_ERROR:
    case FailState::FILE_BLOCKED:
    case FailState::FILE_SECURITY_CHECK_FAILED:
    case FailState::FILE_TOO_SHORT:
    case FailState::FILE_HASH_MISMATCH:
    case FailState::FILE_SAME_AS_SOURCE:
    case FailState::NETWORK_FAILED:
    case FailState::NETWORK_TIMEOUT:
    case FailState::NETWORK_DISCONNECTED:
    case FailState::NETWORK_SERVER_DOWN:
    case FailState::NETWORK_INVALID_REQUEST:
    case FailState::SERVER_FAILED:
    case FailState::SERVER_NO_RANGE:
    case FailState::SERVER_BAD_CONTENT:
    case FailState::SERVER_UNAUTHORIZED:
    case FailState::SERVER_CERT_PROBLEM:
    case FailState::SERVER_FORBIDDEN:
    case FailState::SERVER_UNREACHABLE:
    case FailState::SERVER_CONTENT_LENGTH_MISMATCH:
    case FailState::SERVER_CROSS_ORIGIN_REDIRECT:
    case FailState::USER_CANCELED:
    case FailState::USER_SHUTDOWN:
    case FailState::CRASH:
      *fail_state = static_cast<FailState>(value);
      return true;
  }
  return false;
}

}  // namespace offline_items_collection

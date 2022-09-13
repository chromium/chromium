// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/url_request_error.h"

#include "net/base/net_errors.h"

namespace cronet {

UrlRequestError NetErrorToUrlRequestError(int net_error) {
  switch (net_error) {
    case net::ERR_NAME_NOT_RESOLVED:
      return HOSTNAME_NOT_RESOLVED;
    case net::ERR_INTERNET_DISCONNECTED:
      return INTERNET_DISCONNECTED;
    case net::ERR_NETWORK_CHANGED:
      return NETWORK_CHANGED;
    case net::ERR_TIMED_OUT:
      return TIMED_OUT;
    case net::ERR_CONNECTION_CLOSED:
      return CONNECTION_CLOSED;
    case net::ERR_CONNECTION_TIMED_OUT:
      return CONNECTION_TIMED_OUT;
    case net::ERR_CONNECTION_REFUSED:
      return CONNECTION_REFUSED;
    case net::ERR_CONNECTION_RESET:
      return CONNECTION_RESET;
    case net::ERR_ADDRESS_UNREACHABLE:
      return ADDRESS_UNREACHABLE;
    case net::ERR_QUIC_PROTOCOL_ERROR:
      return QUIC_PROTOCOL_FAILED;
    default:
      return OTHER;
  }
}

}  // namespace cronet

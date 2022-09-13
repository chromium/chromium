// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_URL_REQUEST_ERROR_H_
#define COMPONENTS_CRONET_ANDROID_URL_REQUEST_ERROR_H_

namespace cronet {

// Error codes for the most popular network stack error codes.
// For descriptions see corresponding constants in UrlRequestException.java.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net.impl
enum UrlRequestError {
  LISTENER_EXCEPTION_THROWN,
  HOSTNAME_NOT_RESOLVED,
  INTERNET_DISCONNECTED,
  NETWORK_CHANGED,
  TIMED_OUT,
  CONNECTION_CLOSED,
  CONNECTION_TIMED_OUT,
  CONNECTION_REFUSED,
  CONNECTION_RESET,
  ADDRESS_UNREACHABLE,
  QUIC_PROTOCOL_FAILED,
  OTHER,
};

// Converts most popular net::ERR_* values to counterparts accessible in Java.
UrlRequestError NetErrorToUrlRequestError(int net_error);

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_URL_REQUEST_ERROR_H_

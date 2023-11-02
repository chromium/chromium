// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_HTTP_HTTP_STATUS_CODES_H_
#define CHROME_CHROME_CLEANER_HTTP_HTTP_STATUS_CODES_H_

// Useful Http status codes. Adapted from net/http/http_status_code_list.h.
enum class HttpStatus {
  // Informational 1xx
  kContinue = 100,
  kSwitchingProtocols = 101,

  // Successful 2xx
  kOk = 200,
  kCreated = 201,
  kAccepted = 202,
  kNonAuthoritativeInformation = 203,
  kNoContent = 204,
  kResetContent = 205,
  kPartialContent = 206,

  // Redirection 3xx
  kMultipleChoices = 300,
  kMovedPermanently = 301,
  kFound = 302,
  kSeeOther = 303,
  kNotModified = 304,
  kUseProxy = 305,
  // 306 is no longer used.
  kTemporaryRedirect = 307,
  kPermanentRedirect = 308,

  // Client error 4xx
  kBadRequest = 400,
  kUnauthorized = 401,
  kPaymentRequired = 402,
  kForbidden = 403,
  kNotFound = 404,
  kMethodNotAllowed = 405,
  kNotAcceptable = 406,
  kProxyAuthenticationRequired = 407,
  kRequestTimeout = 408,
  kConflict = 409,
  kGone = 410,
  kLengthRequired = 411,
  kPreconditionFailed = 412,
  kRequestEntityTooLarge = 413,
  kRequestUriTooLong = 414,
  kUnsupportedMediaType = 415,
  kRequestedRangeNotSatisfiable = 416,
  kExpectationFailed = 417,

  // Server error 5xx
  kInternalServerError = 500,
  kNotImplemented = 501,
  kBadGateway = 502,
  kServiceUnavailable = 503,
  kGatewayTimeout = 504,
  kVersionNotSupported = 505,
};

#endif  // CHROME_CHROME_CLEANER_HTTP_HTTP_STATUS_CODES_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_URL_UTIL_H_
#define COMPONENTS_PAYMENTS_CORE_URL_UTIL_H_

#include "base/macros.h"

class GURL;

namespace payments {

// Use case specific URL validity checker for web payment APIs.
//
// The command line must be initialized to use this class in tests, because it
// checks for --unsafely-treat-insecure-origin-as-secure=<origin> flag. For
// example:
//  base::CommandLine::Init(0, nullptr);
class UrlUtil {
 public:
  // Validation according to https://w3c.github.io/payment-method-id/#validation
  // with exceptions for local development.
  //
  // Returns true if one of the following is true.
  // - HTTPS scheme.
  // - Localhost.
  // - Whitelisted via --unsafely-treat-insecure-origin-as-secure=<origin>.
  //
  // Note that username and password are not valid here. Path (/hello), query
  // (?world), and ref (#foo) are valid. For example:
  // "https://chromium.org/pay".
  static bool IsValidUrlBasedPaymentMethodIdentifier(const GURL& url);

  // Checks whether the given |url| is a valid origin for "supported_origins"
  // field in the payment method manifest.
  //
  // Returns true if one of the following is true.
  // - HTTPS scheme.
  // - Localhost.
  // - Whitelisted via --unsafely-treat-insecure-origin-as-secure=<origin>.
  //
  // Note that username, password, path (/hello), query (?world), and ref (#foo)
  // are not valid here. Only scheme, hostname, and port number are allowed. For
  // example: "https://chromium.org".
  static bool IsValidSupportedOrigin(const GURL& url);

  // Checks whether the given |url| is a valid URL for a payment method manifest
  // or web app manifest that is fetched in the course of a web payment API
  // flow.
  //
  // Returns true if one of the following is true.
  // - Cryptographic scheme.
  // - Localhost.
  // - Whitelisted via --unsafely-treat-insecure-origin-as-secure=<origin>.
  //
  // For example: "https://chromium.org/pay/web-app-manifest.json".
  static bool IsValidManifestUrl(const GURL& url);

  // Checks whether the page at the given |url| should be allowed to use the web
  // payment APIs.
  //
  // Returns true if one of the following is true.
  // - Cryptographic scheme.
  // - File scheme.
  // - Localhost.
  // - Whitelisted via --unsafely-treat-insecure-origin-as-secure=<origin>.
  //
  // For example: "http://localhost:8080".
  static bool IsOriginAllowedToUseWebPaymentApis(const GURL& url);

  // Checks whether the page with the given |url| should be allowed to be
  // displayed in a payment handler window.
  //
  // Returns true if one of the following is true.
  // - about::blank. (Can happen momentarily when opening the window.)
  // - Cryptographic scheme.
  // - File scheme.
  // - Localhost.
  // - Whitelisted via --unsafely-treat-insecure-origin-as-secure=<origin>.
  //
  // For example: "https://chromium.org/pay/confirm-payment.html"
  static bool IsValidUrlInPaymentHandlerWindow(const GURL& url);

  // Checks whether the page at the given |url| would typically be used for
  // local development, e.g., localhost. The |url| parameter should be a valid
  // GURL.
  //
  // Returns true if one of the following is true.
  // - File scheme.
  // - Localhost.
  // - Whitelisted via --unsafely-treat-insecure-origin-as-secure=<origin>.
  //
  // For example: "http://localhost:8080".
  static bool IsLocalDevelopmentUrl(const GURL& url);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UrlUtil);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_URL_UTIL_H_

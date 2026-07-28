// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_UTILITY_PAYMENT_MANIFEST_PARSER_H_
#define COMPONENTS_PAYMENTS_CONTENT_UTILITY_PAYMENT_MANIFEST_PARSER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "components/payments/content/web_app_manifest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class DictValue;
}

namespace payments {

class ErrorLogger;

// Parser for payment method manifests and web app manifests.
//
// Example 1 of valid payment method manifest structure:
//
// {
//   "default_applications": ["payment-app.json"],
//   "supported_origins": ["https://alicepay.com"]
// }
//
// Example 2 of valid payment method manifest structure:
//
// {
//   "default_applications": ["payment-app.json"],
//   "supported_origins": "*"
// }
//
// Example valid web app manifest structure:
//
// {
//   "name": "bobpay",
//   "serviceworker": {
//     "src": "bobpay.js",
//     "scope": "/pay",
//     "use_cache": false
//   },
//  "icons": [{
//    "src": "icon/bobpay.png",
//    "sizes": "48x48",
//    "type": "image/png"
//   },{
//    "src": "icon/lowres",
//    "sizes": "48x48"
//   }],
//   "related_applications": [{
//     "platform": "play",
//     "id": "com.bobpay.app",
//     "min_version": "1",
//     "fingerprint": [{
//       "type": "sha256_cert",
//       "value": "91:5C:88:65:FF:C4:E8:20:CF:F7:3E:C8:64:D0:95:F0:06:19:2E:A6"
//     }]
//   }]
// }
//
// Specs:
// https://developers.google.com/web/fundamentals/payments/payment-apps-developer-guide/web-payment-apps
// https://developers.google.com/web/fundamentals/payments/payment-apps-developer-guide/android-payment-apps
// https://w3c.github.io/payment-method-manifest/
// https://w3c.github.io/manifest/
//
// The command line must be initialized to use this class in tests, because it
// checks for --unsafely-treat-insecure-origin-as-secure=<origin> flag. For
// example:
//  base::CommandLine::Init(0, nullptr);
class PaymentManifestParser {
 public:
  // Web app icon info parsed from web app manifest.
  struct WebAppIcon {
    WebAppIcon();
    ~WebAppIcon();

    std::string src;
    std::string sizes;
    std::string type;
  };

  // TODO(crbug.com/40681786): Return manifest parser errors to caller.

  struct WebAppInstallationInfoResult {
    WebAppInstallationInfoResult();
    ~WebAppInstallationInfoResult();
    WebAppInstallationInfoResult(WebAppInstallationInfoResult&&);
    WebAppInstallationInfoResult& operator=(WebAppInstallationInfoResult&&);

    std::unique_ptr<WebAppInstallationInfo> installation_info;
    std::unique_ptr<std::vector<WebAppIcon>> icons;
  };

  explicit PaymentManifestParser(std::unique_ptr<ErrorLogger> log);

  PaymentManifestParser(const PaymentManifestParser&) = delete;
  PaymentManifestParser& operator=(const PaymentManifestParser&) = delete;

  ~PaymentManifestParser();

  // Parses the payment method manifest. Returns true if the content was valid
  // JSON and a dictionary. Output parameters are populated on success.
  bool ParsePaymentMethodManifest(const GURL& manifest_url,
                                  const std::string& content,
                                  std::vector<GURL>* web_app_manifest_urls,
                                  std::vector<url::Origin>* supported_origins);

  // Parses the web app manifest. Returns parsed sections, or empty vector on
  // failure.
  std::vector<WebAppManifestSection> ParseWebAppManifest(
      const std::string& content);

  // Parses the installation info in the web app manifest.
  WebAppInstallationInfoResult ParseWebAppInstallationInfo(
      const std::string& content);

  // Visible for tests.
  static void ParsePaymentMethodManifestIntoVectors(
      const GURL& manifest_url,
      const base::DictValue& dict,
      const ErrorLogger& log,
      std::vector<GURL>* web_app_manifest_urls,
      std::vector<url::Origin>* supported_origins);

  static bool ParseWebAppManifestIntoVector(
      const base::DictValue& dict,
      const ErrorLogger& log,
      std::vector<WebAppManifestSection>* output);

  static bool ParseWebAppInstallationInfoIntoStructs(
      const base::DictValue& dict,
      const ErrorLogger& log,
      WebAppInstallationInfo* installation_info,
      std::vector<WebAppIcon>* icons);

 private:
  std::unique_ptr<ErrorLogger> log_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_UTILITY_PAYMENT_MANIFEST_PARSER_H_

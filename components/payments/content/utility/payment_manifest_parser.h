// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_UTILITY_PAYMENT_MANIFEST_PARSER_H_
#define COMPONENTS_PAYMENTS_CONTENT_UTILITY_PAYMENT_MANIFEST_PARSER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/payments/content/web_app_manifest.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {

class ErrorLogger;

// Parser for payment method manifests and web app manifests.
//
// Example 1 of valid payment method manifest structure:
//
// {
//   "default_applications": ["https://bobpay.com/payment-app.json"],
//   "supported_origins": ["https://alicepay.com"]
// }
//
// Example 2 of valid payment method manifest structure:
//
// {
//   "default_applications": ["https://bobpay.com/payment-app.json"],
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
// https://docs.google.com/document/d/1izV4uC-tiRJG3JLooqY3YRLU22tYOsLTNq0P_InPJeE
// https://w3c.github.io/manifest/
//
// Note the JSON parsing is done using the DataDecoder (either OOP or in a safe
// environment).
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

  // Called on successful parsing of a payment method manifest. Parse failure
  // results in empty vectors and "false".
  using PaymentMethodCallback = base::OnceCallback<
      void(const std::vector<GURL>&, const std::vector<url::Origin>&, bool)>;
  // Called on successful parsing of a web app manifest. Parse failure results
  // in an empty vector.
  using WebAppCallback =
      base::OnceCallback<void(const std::vector<WebAppManifestSection>&)>;
  // Called on successful parsing of the installation info (name, icons,
  // and serviceworker) in the web app manifest. Parse failure results in a
  // nullptr.
  using WebAppInstallationInfoCallback =
      base::OnceCallback<void(std::unique_ptr<WebAppInstallationInfo>,
                              std::unique_ptr<std::vector<WebAppIcon>>)>;

  explicit PaymentManifestParser(std::unique_ptr<ErrorLogger> log);
  ~PaymentManifestParser();

  void ParsePaymentMethodManifest(const std::string& content,
                                  PaymentMethodCallback callback);
  void ParseWebAppManifest(const std::string& content, WebAppCallback callback);

  // Parses the installation info in the web app manifest |content|. Sends the
  // result back through callback.
  // Refer to:
  // https://www.w3.org/TR/appmanifest/#webappmanifest-dictionary
  void ParseWebAppInstallationInfo(const std::string& content,
                                   WebAppInstallationInfoCallback callback);

  // Visible for tests.
  static void ParsePaymentMethodManifestIntoVectors(
      std::unique_ptr<base::Value> value,
      const ErrorLogger& log,
      std::vector<GURL>* web_app_manifest_urls,
      std::vector<url::Origin>* supported_origins,
      bool* all_origins_supported);

  static bool ParseWebAppManifestIntoVector(
      std::unique_ptr<base::Value> value,
      const ErrorLogger& log,
      std::vector<WebAppManifestSection>* output);

  static bool ParseWebAppInstallationInfoIntoStructs(
      std::unique_ptr<base::Value> value,
      const ErrorLogger& log,
      WebAppInstallationInfo* installation_info,
      std::vector<WebAppIcon>* icons);

 private:
  void OnPaymentMethodParse(PaymentMethodCallback callback,
                            data_decoder::DataDecoder::ValueOrError result);
  void OnWebAppParse(WebAppCallback callback,
                     data_decoder::DataDecoder::ValueOrError result);
  void OnWebAppParseInstallationInfo(
      WebAppInstallationInfoCallback callback,
      data_decoder::DataDecoder::ValueOrError result);

  int64_t parse_payment_callback_counter_ = 0;
  int64_t parse_webapp_callback_counter_ = 0;

  std::unique_ptr<ErrorLogger> log_;
  base::WeakPtrFactory<PaymentManifestParser> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaymentManifestParser);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_UTILITY_PAYMENT_MANIFEST_PARSER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PAYMENTS_PAYMENT_APP_INSTALL_UTIL_H_
#define CHROME_TEST_PAYMENTS_PAYMENT_APP_INSTALL_UTIL_H_

#include <string>

#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments {

class PaymentAppInstallUtil {
 public:
  enum class IconInstall {
    kWithIcon,
    kWithoutIcon,
    kWithLargeIcon,
  };

  // Install the payment app specified by `hostname`, e.g., "a.com".
  //
  // Specify the path of the service worker with `service_worker_file_path`. It
  // must start with a "/".
  //
  // On success, returns the payment method of the installed payment app,
  // e.g., "https://a.com:12345". Returns an empty string on failure.
  static std::string InstallPaymentApp(
      content::WebContents& web_contents,
      net::EmbeddedTestServer& test_server,
      const std::string& hostname,
      const std::string& service_worker_file_path,
      IconInstall icon_install);

  // Install the payment app specified by `service_worker_javascript_file_url`
  // with the given `payment_method_idnetifier`. Returns `true` on success.
  static bool InstallPaymentAppForPaymentMethodIdentifier(
      content::WebContents& web_contents,
      const GURL& service_worker_javascript_file_url,
      const std::string& payment_method_identifier,
      IconInstall icon_install);

  PaymentAppInstallUtil() = delete;
};

}  // namespace payments

#endif  // CHROME_TEST_PAYMENTS_PAYMENT_APP_INSTALL_UTIL_H_

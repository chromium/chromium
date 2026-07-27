// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PAYMENTS_PAYMENT_APP_INSTALL_UTIL_H_
#define CHROME_TEST_PAYMENTS_PAYMENT_APP_INSTALL_UTIL_H_

#include <string>

#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace payments {
namespace test {

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
  //
  // `initiator_frame` is the frame where the payment app installation is
  // initiated.
  static std::string InstallPaymentApp(
      content::RenderFrameHost& initiator_frame,
      net::EmbeddedTestServer& test_server,
      const std::string& hostname,
      const std::string& service_worker_file_path,
      IconInstall icon_install);

  // Install the payment app specified by `service_worker_javascript_file_url`
  // with the given `payment_method_identifier`. Returns `true` on success.
  // `initiator_frame` is the frame where the payment app installation is
  // initiated.
  static bool InstallPaymentAppForPaymentMethodIdentifier(
      content::RenderFrameHost& initiator_frame,
      const GURL& service_worker_javascript_file_url,
      const std::string& payment_method_identifier,
      IconInstall icon_install);

  PaymentAppInstallUtil() = delete;
};

}  // namespace test
}  // namespace payments

#endif  // CHROME_TEST_PAYMENTS_PAYMENT_APP_INSTALL_UTIL_H_

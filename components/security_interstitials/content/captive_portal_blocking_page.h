// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CAPTIVE_PORTAL_BLOCKING_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CAPTIVE_PORTAL_BLOCKING_PAGE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/security_interstitials/content/ssl_blocking_page_base.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace net {
class SSLInfo;
}

// This class is responsible for showing/hiding the interstitial page that is
// shown when a captive portal triggers an SSL error.
// It deletes itself when the interstitial page is closed.
//
// This class should only be used on the UI thread because its implementation
// uses captive_portal::CaptivePortalService, which can only be accessed on the
// UI thread. Only used when ENABLE_CAPTIVE_PORTAL_DETECTION is true.
class CaptivePortalBlockingPage : public SSLBlockingPageBase {
 public:
  // Interstitial type, for testing.
  static const void* const kTypeForTesting;

  // Callback that is invoked when the user has invoked the button to open the
  // login page.
  using OpenLoginCallback =
      base::RepeatingCallback<void(content::WebContents*)>;

  CaptivePortalBlockingPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      const GURL& login_url,
      bool can_show_enhanced_protection_message,
      const net::SSLInfo& ssl_info,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client,
      const OpenLoginCallback& open_login_callback);

  CaptivePortalBlockingPage(const CaptivePortalBlockingPage&) = delete;
  CaptivePortalBlockingPage& operator=(const CaptivePortalBlockingPage&) =
      delete;

  ~CaptivePortalBlockingPage() override;

  // InterstitialPageDelegate method:
  const void* GetTypeForTesting() override;

  void OverrideWifiInfoForTesting(bool is_wifi_connection,
                                  const std::string& wifi_ssid);

 private:
  // Returns true if the connection is a Wi-Fi connection.
  bool IsWifiConnection() const;
  // Returns the SSID of the connected Wi-Fi network, if any.
  std::string GetWiFiSSID() const;

  // SecurityInterstitialPage methods:
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;

  // SecurityInterstitialPage method:
  void CommandReceived(const std::string& command) override;

  OpenLoginCallback open_login_callback_;

  // URL of the login page, opened when the user clicks the "Connect" button.
  // If empty, the default captive portal detection URL for the platform will be
  // used.
  const GURL login_url_;
  const net::SSLInfo ssl_info_;

  bool is_wifi_info_overridden_for_testing_ = false;
  bool is_wifi_connection_for_testing_ = false;
  std::string wifi_ssid_for_testing_;
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CAPTIVE_PORTAL_BLOCKING_PAGE_H_

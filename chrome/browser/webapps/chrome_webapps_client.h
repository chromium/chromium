// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_CHROME_WEBAPPS_CLIENT_H_
#define CHROME_BROWSER_WEBAPPS_CHROME_WEBAPPS_CLIENT_H_

#include "components/webapps/browser/webapps_client.h"

namespace url {
class Origin;
}

namespace webapps {

// Abstract implementation for the common functionality between desktop
// & Android.
class ChromeWebappsClient : public WebappsClient {
 public:
  ChromeWebappsClient() = default;
  ChromeWebappsClient(const ChromeWebappsClient&) = delete;
  ChromeWebappsClient& operator=(const ChromeWebappsClient&) = delete;

  // WebappsClient:
  bool IsOriginConsideredSecure(const url::Origin& origin) override;
  security_state::SecurityLevel GetSecurityLevelForWebContents(
      content::WebContents* web_contents) override;
  infobars::ContentInfoBarManager* GetInfoBarManagerForWebContents(
      content::WebContents* web_contents) override;
  std::optional<webapps::AppId> GetAppIdForWebContents(
      content::WebContents* web_contents) override;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_WEBAPPS_CHROME_WEBAPPS_CLIENT_H_

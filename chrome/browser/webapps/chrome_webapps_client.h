// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_CHROME_WEBAPPS_CLIENT_H_
#define CHROME_BROWSER_WEBAPPS_CHROME_WEBAPPS_CLIENT_H_

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/webapps/browser/webapps_client.h"

namespace url {
class Origin;
}

namespace webapps {

class ChromeWebappsClient : public WebappsClient {
 public:
  ChromeWebappsClient(const ChromeWebappsClient&) = delete;
  ChromeWebappsClient& operator=(const ChromeWebappsClient&) = delete;

  static ChromeWebappsClient* GetInstance();

  // WebappsClient:
  bool IsOriginConsideredSecure(const url::Origin& origin) override;
  security_state::SecurityLevel GetSecurityLevelForWebContents(
      content::WebContents* web_contents) override;
  infobars::ContentInfoBarManager* GetInfoBarManagerForWebContents(
      content::WebContents* web_contents) override;
  WebappInstallSource GetInstallSource(content::WebContents* web_contents,
                                       InstallTrigger trigger) override;
  AppBannerManager* GetAppBannerManager(
      content::WebContents* web_contents) override;
#if BUILDFLAG(IS_ANDROID)
  bool IsInstallationInProgress(content::WebContents* web_contents,
                                const GURL& manifest_id) override;
  bool CanShowAppBanners(content::WebContents* web_contents) override;
  void OnWebApkInstallInitiatedFromAppMenu(
      content::WebContents* web_contents) override;
  void InstallWebApk(content::WebContents* web_contents,
                     const AddToHomescreenParams& params) override;
  void InstallShortcut(content::WebContents* web_contents,
                       const AddToHomescreenParams& params) override;
#endif

 private:
  friend base::NoDestructor<ChromeWebappsClient>;

  ChromeWebappsClient() = default;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_WEBAPPS_CHROME_WEBAPPS_CLIENT_H_

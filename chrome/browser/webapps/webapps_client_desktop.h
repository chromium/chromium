// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_WEBAPPS_CLIENT_DESKTOP_H_
#define CHROME_BROWSER_WEBAPPS_WEBAPPS_CLIENT_DESKTOP_H_

#include "base/auto_reset.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/webapps/chrome_webapps_client.h"

#if BUILDFLAG(IS_ANDROID)
#error "Desktop implementation should not be included in Android builds."
#endif

namespace webapps {

class WebappsClientDesktop : public ChromeWebappsClient {
 public:
  WebappsClientDesktop(const WebappsClientDesktop&) = delete;
  WebappsClientDesktop& operator=(const WebappsClientDesktop&) = delete;

  // Creates the singleton instance accessible from WebappsClient::Get().
  static void CreateSingleton();

  // WebappsClient:
  WebappInstallSource GetInstallSource(content::WebContents* web_contents,
                                       InstallTrigger trigger) override;
  AppBannerManager* GetAppBannerManager(
      content::WebContents* web_contents) override;
  // Allows installation if there is no app controlling the start_url. If there
  // is, will still allow installation if:
  // - The manifest_id matches the existing installation, and the existing
  //   installation has the user display mode as kBrowser. (this allows us to
  //   upgrade to a standalone experience through a reinstall).
  // - The controlling app is a DIY app.
  bool DoesNewWebAppConflictWithExistingInstallation(
      content::BrowserContext* browsing_context,
      const GURL& start_url,
      const ManifestId& manifest_id) const override;
  bool IsInAppBrowsingContext(
      content::WebContents* web_contents) const override;
  bool IsAppPartiallyInstalledForSiteUrl(
      content::BrowserContext* browsing_context,
      const GURL& site_url) const override;
  bool IsAppFullyInstalledForSiteUrl(content::BrowserContext* browsing_context,
                                     const GURL& site_url) const override;
  bool IsUrlControlledBySeenManifest(content::BrowserContext* browsing_context,
                                     const GURL& site_url) const override;
  void OnManifestSeen(content::BrowserContext* browsing_context,
                      const blink::mojom::Manifest& manifest) const override;
  void SaveInstallationIgnoredForMl(content::BrowserContext* browsing_context,
                                    const GURL& manifest_id) const override;
  void SaveInstallationDismissedForMl(content::BrowserContext* browsing_context,
                                      const GURL& manifest_id) const override;
  void SaveInstallationAcceptedForMl(content::BrowserContext* browsing_context,
                                     const GURL& manifest_id) const override;
  bool IsMlPromotionBlockedByHistoryGuardrail(
      content::BrowserContext* browsing_context,
      const GURL& manifest_id) const override;
  segmentation_platform::SegmentationPlatformService*
  GetSegmentationPlatformService(
      content::BrowserContext* browsing_context) const override;
  std::optional<webapps::AppId> GetAppIdForWebContents(
      content::WebContents* web_contents) override;

 private:
  friend base::NoDestructor<WebappsClientDesktop>;

  WebappsClientDesktop() = default;

  raw_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_platform_service_for_testing_;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_WEBAPPS_WEBAPPS_CLIENT_DESKTOP_H_

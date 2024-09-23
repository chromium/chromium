// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_WEBAPPS_CLIENT_ANDROID_H_
#define CHROME_BROWSER_WEBAPPS_WEBAPPS_CLIENT_ANDROID_H_

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/webapps/chrome_webapps_client.h"
#include "content/public/browser/browser_context.h"

#if !BUILDFLAG(IS_ANDROID)
#error "Android implementation should not be included for non-Android builds."
#endif

namespace webapps {

class WebappsClientAndroid : public ChromeWebappsClient {
 public:
  // Creates the singleton instance accessible from WebappsClient::Get().
  static void CreateSingleton();

  WebappsClientAndroid(const WebappsClientAndroid&) = delete;
  WebappsClientAndroid& operator=(const WebappsClientAndroid&) = delete;

  // WebappsClient:
  WebappInstallSource GetInstallSource(content::WebContents* web_contents,
                                       InstallTrigger trigger) override;
  AppBannerManager* GetAppBannerManager(
      content::WebContents* web_contents) override;

  // Non-locally installed apps do not exist on Android.
  bool DoesNewWebAppConflictWithExistingInstallation(
      content::BrowserContext* browsing_context,
      const GURL& start_url,
      const ManifestId& manifest_id) const override;

  // TODO(crbug.com/40269982): Implement.
  bool IsInAppBrowsingContext(
      content::WebContents* web_contents) const override;
  // TODO(crbug.com/40269982): Implement.
  bool IsAppPartiallyInstalledForSiteUrl(
      content::BrowserContext* browsing_context,
      const GURL& site_url) const override;
  // TODO(crbug.com/40269982): Implement.
  bool IsAppFullyInstalledForSiteUrl(content::BrowserContext* browsing_context,
                                     const GURL& site_url) const override;
  // TODO(crbug.com/40269982): Implement.
  bool IsUrlControlledBySeenManifest(content::BrowserContext* browsing_context,
                                     const GURL& site_url) const override;

  void OnManifestSeen(content::BrowserContext* browsing_context,
                      const blink::mojom::Manifest& manifest) const override;

  // TODO(crbug.com/40269982): Implement.
  void SaveInstallationDismissedForMl(content::BrowserContext* browsing_context,
                                      const GURL& manifest_id) const override;
  // TODO(crbug.com/40269982): Implement.
  void SaveInstallationIgnoredForMl(content::BrowserContext* browsing_context,
                                    const GURL& manifest_id) const override;
  // TODO(crbug.com/40269982): Implement.
  void SaveInstallationAcceptedForMl(content::BrowserContext* browsing_context,
                                     const GURL& manifest_id) const override;
  // TODO(crbug.com/40269982): Implement.
  bool IsMlPromotionBlockedByHistoryGuardrail(
      content::BrowserContext* browsing_context,
      const GURL& manifest_id) const override;
  // TODO(crbug.com/40269982): Implement.
  segmentation_platform::SegmentationPlatformService*
  GetSegmentationPlatformService(
      content::BrowserContext* browsing_context) const override;

  bool IsInstallationInProgress(content::WebContents* web_contents,
                                const GURL& manifest_id) override;
  bool CanShowAppBanners(const content::WebContents* web_contents) override;
  void OnWebApkInstallInitiatedFromAppMenu(
      content::WebContents* web_contents) override;
  void InstallWebApk(content::WebContents* web_contents,
                     const AddToHomescreenParams& params) override;
  void InstallShortcut(content::WebContents* web_contents,
                       const AddToHomescreenParams& params) override;

 private:
  friend base::NoDestructor<WebappsClientAndroid>;

  bool IsInstallationInProgress(content::BrowserContext* browser_context,
                                const GURL& manifest_id) const;

  WebappsClientAndroid() = default;
};

}  // namespace webapps

#endif  // CHROME_BROWSER_WEBAPPS_WEBAPPS_CLIENT_ANDROID_H_

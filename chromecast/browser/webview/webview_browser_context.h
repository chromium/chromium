// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_BROWSER_CONTEXT_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_BROWSER_CONTEXT_H_

#include "content/public/browser/browser_context.h"

namespace chromecast {

// This provides an incognito browser context for webviews to use.
class WebviewBrowserContext : public content::BrowserContext {
 public:
  explicit WebviewBrowserContext(content::BrowserContext* main_browser_context);
  ~WebviewBrowserContext() override;

  WebviewBrowserContext(const WebviewBrowserContext&) = delete;
  void operator=(const WebviewBrowserContext&) = delete;

  // BrowserContext implementation:
  base::FilePath GetPath() override;
  bool IsOffTheRecord() override;
  content::ResourceContext* GetResourceContext() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PlatformNotificationService* GetPlatformNotificationService()
      override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::StorageNotificationService* GetStorageNotificationService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath&) override;

 private:
  class ResourceContext;

  content::BrowserContext* main_browser_context_;

  std::unique_ptr<ResourceContext> resource_context_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_BROWSER_CONTEXT_H_

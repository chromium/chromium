// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_browser_context.h"
#include "base/files/file_path.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"

namespace chromecast {

class WebviewBrowserContext::ResourceContext : public content::ResourceContext {
 public:
  ResourceContext() {}
  ~ResourceContext() override {}

  ResourceContext(const ResourceContext&) = delete;
  void operator=(const ResourceContext&) = delete;
};

WebviewBrowserContext::WebviewBrowserContext(
    content::BrowserContext* main_browser_context)
    : main_browser_context_(main_browser_context),
      resource_context_(std::make_unique<ResourceContext>()) {
  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kIncognito);
}

WebviewBrowserContext::~WebviewBrowserContext() {
  NotifyWillBeDestroyed();
  ShutdownStoragePartitions();
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);
  content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                 resource_context_.release());
}

base::FilePath WebviewBrowserContext::GetPath() {
  return base::FilePath();
}

bool WebviewBrowserContext::IsOffTheRecord() {
  return true;
}

content::ResourceContext* WebviewBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

content::DownloadManagerDelegate*
WebviewBrowserContext::GetDownloadManagerDelegate() {
  return main_browser_context_->GetDownloadManagerDelegate();
}

content::BrowserPluginGuestManager* WebviewBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy*
WebviewBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PlatformNotificationService*
WebviewBrowserContext::GetPlatformNotificationService() {
  return nullptr;
}

content::PushMessagingService*
WebviewBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
WebviewBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate*
WebviewBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
WebviewBrowserContext::GetPermissionControllerDelegate() {
  return main_browser_context_->GetPermissionControllerDelegate();
}

content::ClientHintsControllerDelegate*
WebviewBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
WebviewBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
WebviewBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
WebviewBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

std::unique_ptr<content::ZoomLevelDelegate>
WebviewBrowserContext::CreateZoomLevelDelegate(const base::FilePath&) {
  return nullptr;
}

}  // namespace chromecast

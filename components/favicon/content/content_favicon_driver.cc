// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_driver.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/favicon/content/favicon_url_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/favicon_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/image/image.h"

namespace favicon {

gfx::Image ContentFaviconDriver::GetFavicon() const {
  // Like GetTitle(), we also want to use the favicon for the last committed
  // entry rather than a pending navigation entry.
  content::NavigationController& controller = web_contents()->GetController();

  content::NavigationEntry* entry = controller.GetLastCommittedEntry();
  if (entry)
    return entry->GetFavicon().image;
  return gfx::Image();
}

bool ContentFaviconDriver::FaviconIsValid() const {
  content::NavigationController& controller = web_contents()->GetController();

  content::NavigationEntry* entry = controller.GetLastCommittedEntry();
  if (entry)
    return entry->GetFavicon().valid;

  return false;
}

GURL ContentFaviconDriver::GetActiveURL() {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  return entry ? entry->GetURL() : GURL();
}

GURL ContentFaviconDriver::GetManifestURL(content::RenderFrameHost* rfh) {
  DocumentManifestData* document_data =
      DocumentManifestData::GetOrCreateForCurrentDocument(rfh);
  return document_data->has_manifest_url
             ? rfh->GetPage().GetManifestUrl().value_or(GURL())
             : GURL();
}

ContentFaviconDriver::ContentFaviconDriver(content::WebContents* web_contents,
                                           CoreFaviconService* favicon_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContentFaviconDriver>(*web_contents),
      FaviconDriverImpl(favicon_service) {}

ContentFaviconDriver::~ContentFaviconDriver() = default;

ContentFaviconDriver::DocumentManifestData::DocumentManifestData(
    content::RenderFrameHost* rfh)
    : content::DocumentUserData<DocumentManifestData>(rfh) {}
ContentFaviconDriver::DocumentManifestData::~DocumentManifestData() = default;

ContentFaviconDriver::NavigationManifestData::NavigationManifestData(
    content::NavigationHandle& navigation_handle) {}
ContentFaviconDriver::NavigationManifestData::~NavigationManifestData() =
    default;

void ContentFaviconDriver::OnDidDownloadManifest(
    ManifestDownloadCallback callback,
    blink::mojom::ManifestRequestResult result,
    const GURL& manifest_url,
    blink::mojom::ManifestPtr manifest) {
  // ~WebContentsImpl triggers running any pending callbacks for manifests.
  // As we're about to be destroyed ignore the request. To do otherwise may
  // result in calling back to this and attempting to use the WebContents, which
  // will crash.
  if (!web_contents())
    return;

  std::vector<FaviconURL> candidates;
  if (manifest) {
    for (const auto& icon : manifest->icons) {
      candidates.emplace_back(
          icon.src, favicon_base::IconType::kWebManifestIcon, icon.sizes);
    }
  }
  std::move(callback).Run(candidates);
}

int ContentFaviconDriver::DownloadImage(const GURL& url,
                                        int max_image_size,
                                        ImageDownloadCallback callback) {
  bool bypass_cache = (bypass_cache_page_url_ == GetActiveURL());
  bypass_cache_page_url_ = GURL();

  const gfx::Size preferred_size(max_image_size, max_image_size);
  return web_contents()->DownloadImage(url, true, preferred_size,
                                       /*max_bitmap_size=*/max_image_size,
                                       bypass_cache, std::move(callback));
}

void ContentFaviconDriver::DownloadManifest(const GURL& url,
                                            ManifestDownloadCallback callback) {
  // TODO(crbug.com/40762256): This appears to be reachable from pages other
  // than the primary page. This code should likely be refactored so that either
  // this is unreachable from other pages, or the correct page is plumbed in
  // here.
  web_contents()->GetPrimaryPage().GetManifest(
      base::BindOnce(&ContentFaviconDriver::OnDidDownloadManifest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool ContentFaviconDriver::IsOffTheRecord() {
  DCHECK(web_contents());
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

void ContentFaviconDriver::OnFaviconUpdated(
    const GURL& page_url,
    FaviconDriverObserver::NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  DCHECK(entry);
  DCHECK_EQ(entry->GetURL(), page_url);

  if (notification_icon_type == FaviconDriverObserver::NON_TOUCH_16_DIP) {
    entry->GetFavicon().valid = true;
    entry->GetFavicon().url = icon_url;
    entry->GetFavicon().image = image;
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }

  NotifyFaviconUpdatedObservers(notification_icon_type, icon_url,
                                icon_url_changed, image);
}

void ContentFaviconDriver::OnFaviconDeleted(
    const GURL& page_url,
    FaviconDriverObserver::NotificationIconType notification_icon_type) {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  DCHECK(entry && entry->GetURL() == page_url);

  if (notification_icon_type == FaviconDriverObserver::NON_TOUCH_16_DIP) {
    entry->GetFavicon() = content::FaviconStatus();
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }

  NotifyFaviconUpdatedObservers(notification_icon_type, /*icon_url=*/GURL(),
                                /*icon_url_changed=*/true,
                                content::FaviconStatus().image);
}

void ContentFaviconDriver::DidUpdateFaviconURL(
    content::RenderFrameHost* rfh,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  // Ignore the update if there is no last committed navigation entry. This can
  // occur when loading an initially blank page.
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();

  if (!entry)
    return;

  if (!rfh->IsDocumentOnLoadCompletedInMainFrame())
    return;

  OnUpdateCandidates(rfh->GetLastCommittedURL(),
                     FaviconURLsFromContentFaviconURLs(candidates),
                     GetManifestURL(rfh));
}

void ContentFaviconDriver::DidUpdateWebManifestURL(
    content::RenderFrameHost* rfh,
    const GURL& manifest_url) {
  // Ignore the update if there is no last committed navigation entry. This can
  // occur when loading an initially blank page.
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry || !rfh->IsDocumentOnLoadCompletedInMainFrame())
    return;

  DocumentManifestData* document_data =
      DocumentManifestData::GetOrCreateForCurrentDocument(rfh);
  document_data->has_manifest_url = true;

  // On regular page loads, DidUpdateManifestURL() is guaranteed to be called
  // before DidUpdateFaviconURL(). However, a page can update the favicons via
  // javascript.
  if (!rfh->FaviconURLs().empty()) {
    OnUpdateCandidates(rfh->GetLastCommittedURL(),
                       FaviconURLsFromContentFaviconURLs(rfh->FaviconURLs()),
                       GetManifestURL(rfh));
  }
}

void ContentFaviconDriver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  content::ReloadType reload_type = navigation_handle->GetReloadType();
  if (reload_type == content::ReloadType::NONE || IsOffTheRecord())
    return;

  if (!navigation_handle->IsSameDocument()) {
    NavigationManifestData* navigation_data =
        NavigationManifestData::GetOrCreateForNavigationHandle(
            *navigation_handle);
    navigation_data->has_manifest_url = false;
  }

  if (reload_type == content::ReloadType::BYPASSING_CACHE)
    bypass_cache_page_url_ = navigation_handle->GetURL();
 
  SetFaviconOutOfDateForPage(
      navigation_handle->GetURL(),
      reload_type == content::ReloadType::BYPASSING_CACHE);
}

void ContentFaviconDriver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    return;
  }

  // Transfer in-flight navigation data to the document user data.
  NavigationManifestData* navigation_data =
      NavigationManifestData::GetOrCreateForNavigationHandle(
          *navigation_handle);
  DocumentManifestData* document_data =
      DocumentManifestData::GetOrCreateForCurrentDocument(
          navigation_handle->GetRenderFrameHost());
  document_data->has_manifest_url = navigation_data->has_manifest_url;

  // Wait till the user navigates to a new URL to start checking the cache
  // again. The cache may be ignored for non-reload navigations (e.g.
  // history.replace() in-page navigation). This is allowed to increase the
  // likelihood that "reloading a page ignoring the cache" redownloads the
  // favicon. In particular, a page may do an in-page navigation before
  // FaviconHandler has the time to determine that the favicon needs to be
  // redownloaded.
  GURL url = navigation_handle->GetURL();
  if (url != bypass_cache_page_url_)
    bypass_cache_page_url_ = GURL();

  // Get the favicon, either from history or request it from the net.
  FetchFavicon(url, navigation_handle->IsSameDocument());
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(
    ContentFaviconDriver::NavigationManifestData);
DOCUMENT_USER_DATA_KEY_IMPL(ContentFaviconDriver::DocumentManifestData);
WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentFaviconDriver);

}  // namespace favicon

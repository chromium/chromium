// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_driver.h"

#include "base/bind.h"
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
#include "content/public/common/favicon_url.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "ui/gfx/image/image.h"

namespace favicon {
namespace {

void ExtractManifestIcons(
    ContentFaviconDriver::ManifestDownloadCallback callback,
    const GURL& manifest_url,
    const blink::Manifest& manifest) {
  std::vector<FaviconURL> candidates;
  for (const auto& icon : manifest.icons) {
    candidates.emplace_back(icon.src, favicon_base::IconType::kWebManifestIcon,
                            icon.sizes);
  }
  std::move(callback).Run(candidates);
}

}  // namespace

// static
void ContentFaviconDriver::CreateForWebContents(
    content::WebContents* web_contents,
    FaviconService* favicon_service) {
  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new ContentFaviconDriver(
                                web_contents, favicon_service)));
}

void ContentFaviconDriver::SaveFaviconEvenIfInIncognito() {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return;

  // Make sure the page is in history, otherwise adding the favicon does
  // nothing.
  GURL page_url = entry->GetURL();
  favicon_service()->AddPageNoVisitForBookmark(page_url, entry->GetTitle());

  const content::FaviconStatus& favicon_status = entry->GetFavicon();
  if (!favicon_service() || !favicon_status.valid ||
      favicon_status.url.is_empty() || favicon_status.image.IsEmpty()) {
    return;
  }

  favicon_service()->SetFavicons({page_url}, favicon_status.url,
                                 favicon_base::IconType::kFavicon,
                                 favicon_status.image);
}

gfx::Image ContentFaviconDriver::GetFavicon() const {
  // Like GetTitle(), we also want to use the favicon for the last committed
  // entry rather than a pending navigation entry.
  content::NavigationController& controller = web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetTransientEntry();
  if (entry)
    return entry->GetFavicon().image;

  entry = controller.GetLastCommittedEntry();
  if (entry)
    return entry->GetFavicon().image;
  return gfx::Image();
}

bool ContentFaviconDriver::FaviconIsValid() const {
  content::NavigationController& controller = web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetTransientEntry();
  if (entry)
    return entry->GetFavicon().valid;

  entry = controller.GetLastCommittedEntry();
  if (entry)
    return entry->GetFavicon().valid;

  return false;
}

GURL ContentFaviconDriver::GetActiveURL() {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  return entry ? entry->GetURL() : GURL();
}

ContentFaviconDriver::ContentFaviconDriver(content::WebContents* web_contents,
                                           FaviconService* favicon_service)
    : content::WebContentsObserver(web_contents),
      FaviconDriverImpl(favicon_service),
      document_on_load_completed_(false) {}

ContentFaviconDriver::~ContentFaviconDriver() {
}

int ContentFaviconDriver::DownloadImage(const GURL& url,
                                        int max_image_size,
                                        ImageDownloadCallback callback) {
  bool bypass_cache = (bypass_cache_page_url_ == GetActiveURL());
  bypass_cache_page_url_ = GURL();

  return web_contents()->DownloadImage(
      url, true, /*preferred_size=*/max_image_size,
      /*max_bitmap_size=*/max_image_size, bypass_cache, std::move(callback));
}

void ContentFaviconDriver::DownloadManifest(const GURL& url,
                                            ManifestDownloadCallback callback) {
  web_contents()->GetManifest(
      base::BindOnce(&ExtractManifestIcons, std::move(callback)));
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
    const std::vector<content::FaviconURL>& candidates) {
  // Ignore the update if there is no last committed navigation entry. This can
  // occur when loading an initially blank page.
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return;

  // We update |favicon_urls_| even if the list is believed to be partial
  // (checked below), because callers of our getter favicon_urls() expect so.
  favicon_urls_ = candidates;

  if (!document_on_load_completed_)
    return;

  OnUpdateCandidates(entry->GetURL(),
                     FaviconURLsFromContentFaviconURLs(candidates),
                     manifest_url_);
}

void ContentFaviconDriver::DidUpdateWebManifestURL(
    const base::Optional<GURL>& manifest_url) {
  // Ignore the update if there is no last committed navigation entry. This can
  // occur when loading an initially blank page.
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry || !document_on_load_completed_)
    return;

  manifest_url_ = manifest_url.value_or(GURL());

  // On regular page loads, DidUpdateManifestURL() is guaranteed to be called
  // before DidUpdateFaviconURL(). However, a page can update the favicons via
  // javascript.
  if (favicon_urls_.has_value()) {
    OnUpdateCandidates(entry->GetURL(),
                       FaviconURLsFromContentFaviconURLs(*favicon_urls_),
                       manifest_url_);
  }
}

void ContentFaviconDriver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  favicon_urls_.reset();

  if (!navigation_handle->IsSameDocument()) {
    document_on_load_completed_ = false;
    manifest_url_ = GURL();
  }

  content::ReloadType reload_type = navigation_handle->GetReloadType();
  if (reload_type == content::ReloadType::NONE || IsOffTheRecord())
    return;

  bypass_cache_page_url_ = navigation_handle->GetURL();
  SetFaviconOutOfDateForPage(
      navigation_handle->GetURL(),
      reload_type == content::ReloadType::BYPASSING_CACHE);
}

void ContentFaviconDriver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    return;
  }

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

void ContentFaviconDriver::DocumentOnLoadCompletedInMainFrame() {
  document_on_load_completed_ = true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentFaviconDriver)

}  // namespace favicon

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/ios/web_favicon_driver.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/favicon/core/favicon_url.h"
#include "components/favicon/ios/favicon_url_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/favicon/favicon_status.h"
#include "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "skia/ext/skia_utils_ios.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace favicon {

// static
void WebFaviconDriver::CreateForWebState(web::WebState* web_state,
                                         FaviconService* favicon_service) {
  if (FromWebState(web_state))
    return;

  web_state->SetUserData(UserDataKey(), base::WrapUnique(new WebFaviconDriver(
                                            web_state, favicon_service)));
}

gfx::Image WebFaviconDriver::GetFavicon() const {
  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetLastCommittedItem();
  return item ? item->GetFavicon().image : gfx::Image();
}

bool WebFaviconDriver::FaviconIsValid() const {
  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetLastCommittedItem();
  return item ? item->GetFavicon().valid : false;
}

GURL WebFaviconDriver::GetActiveURL() {
  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetLastCommittedItem();
  return item ? item->GetURL() : GURL();
}

int WebFaviconDriver::DownloadImage(const GURL& url,
                                    int max_image_size,
                                    ImageDownloadCallback callback) {
  static int downloaded_image_count = 0;
  int local_download_id = ++downloaded_image_count;

  GURL local_url(url);
  __block ImageDownloadCallback local_callback = std::move(callback);

  image_fetcher::ImageDataFetcherBlock ios_callback =
      ^(NSData* data, const image_fetcher::RequestMetadata& metadata) {
        if (metadata.http_response_code ==
            image_fetcher::RequestMetadata::RESPONSE_CODE_INVALID)
          return;

        std::vector<SkBitmap> frames;
        std::vector<gfx::Size> sizes;
        if (data) {
          frames = skia::ImageDataToSkBitmaps(data);
          for (const auto& frame : frames) {
            sizes.push_back(gfx::Size(frame.width(), frame.height()));
          }
        }
        std::move(local_callback)
            .Run(local_download_id, metadata.http_response_code, local_url,
                 frames, sizes);
      };
  image_fetcher_.FetchImageDataWebpDecoded(url, ios_callback);

  return downloaded_image_count;
}

void WebFaviconDriver::DownloadManifest(const GURL& url,
                                        ManifestDownloadCallback callback) {
  NOTREACHED();
}

bool WebFaviconDriver::IsOffTheRecord() {
  DCHECK(web_state_);
  return web_state_->GetBrowserState()->IsOffTheRecord();
}

void WebFaviconDriver::OnFaviconUpdated(
    const GURL& page_url,
    FaviconDriverObserver::NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  // Check whether the active URL has changed since FetchFavicon() was called.
  // On iOS, the active URL can change between calls to FetchFavicon(). For
  // instance, FetchFavicon() is not synchronously called when the active URL
  // changes as a result of CRWSessionController::goToEntry().
  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetVisibleItem();
  if (!item || item->GetURL() != page_url)
    return;

  web::FaviconStatus& favicon_status = item->GetFavicon();
  favicon_status.valid = true;
  favicon_status.image = image;
  favicon_status.url = icon_url;

  NotifyFaviconUpdatedObservers(notification_icon_type, icon_url,
                                icon_url_changed, image);
}

void WebFaviconDriver::OnFaviconDeleted(
    const GURL& page_url,
    FaviconDriverObserver::NotificationIconType notification_icon_type) {
  // Check whether the active URL has changed since FetchFavicon() was called.
  // On iOS, the active URL can change between calls to FetchFavicon(). For
  // instance, FetchFavicon() is not synchronously called when the active URL
  // changes as a result of CRWSessionController::goToEntry().
  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetVisibleItem();
  if (!item || item->GetURL() != page_url)
    return;

  item->GetFavicon() = web::FaviconStatus();

  NotifyFaviconUpdatedObservers(notification_icon_type, /*icon_url=*/GURL(),
                                /*icon_url_changed=*/true,
                                item->GetFavicon().image);
}

WebFaviconDriver::WebFaviconDriver(web::WebState* web_state,
                                   FaviconService* favicon_service)
    : FaviconDriverImpl(favicon_service),
      image_fetcher_(web_state->GetBrowserState()->GetSharedURLLoaderFactory()),
      web_state_(web_state) {
  web_state_->AddObserver(this);
}

WebFaviconDriver::~WebFaviconDriver() {
  // WebFaviconDriver is owned by WebState (as it is a WebStateUserData), so
  // the WebStateDestroyed will be called before the destructor and the member
  // field reset to null.
  DCHECK(!web_state_);
}

void WebFaviconDriver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  FetchFavicon(web_state->GetLastCommittedURL(),
               navigation_context->IsSameDocument());
}

void WebFaviconDriver::FaviconUrlUpdated(
    web::WebState* web_state,
    const std::vector<web::FaviconURL>& candidates) {
  DCHECK_EQ(web_state_, web_state);
  DCHECK(!candidates.empty());
  OnUpdateCandidates(GetActiveURL(), FaviconURLsFromWebFaviconURLs(candidates),
                     GURL());
}

void WebFaviconDriver::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(WebFaviconDriver)

}  // namespace favicon

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/ios/web_favicon_driver.h"

#include "base/functional/bind.h"
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

namespace favicon {

gfx::Image WebFaviconDriver::GetFavicon() const {
  return web_state_->GetFaviconStatus().image;
}

bool WebFaviconDriver::FaviconIsValid() const {
  return web_state_->GetFaviconStatus().valid;
}

GURL WebFaviconDriver::GetActiveURL() {
  return web_state_->GetLastCommittedURL();
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
          frames = skia::ImageDataToSkBitmapsWithMaxSize(data, max_image_size);
          for (const auto& frame : frames) {
            sizes.push_back(gfx::Size(frame.width(), frame.height()));
          }
          DCHECK_EQ(frames.size(), sizes.size());
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
  NOTREACHED_IN_MIGRATION();
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
  web::FaviconStatus favicon_status;
  favicon_status.valid = true;
  favicon_status.image = image;
  favicon_status.url = icon_url;

  SetFaviconStatus(page_url, favicon_status, notification_icon_type,
                   icon_url_changed);
}

void WebFaviconDriver::OnFaviconDeleted(
    const GURL& page_url,
    FaviconDriverObserver::NotificationIconType notification_icon_type) {
  SetFaviconStatus(page_url, web::FaviconStatus(), notification_icon_type,
                   /*icon_url_changed=*/true);
}

WebFaviconDriver::WebFaviconDriver(web::WebState* web_state,
                                   CoreFaviconService* favicon_service)
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

void WebFaviconDriver::SetFaviconStatus(
    const GURL& page_url,
    const web::FaviconStatus& favicon_status,
    FaviconDriverObserver::NotificationIconType notification_icon_type,
    bool icon_url_changed) {
  // Check whether the active URL has changed since FetchFavicon() was called.
  // On iOS, the active URL can change between calls to FetchFavicon(). For
  // instance, FetchFavicon() is not synchronously called when the active URL
  // changes as a result of CRWSessionController::goToEntry().
  if (web_state_->GetLastCommittedURL() != page_url)
    return;

  web_state_->SetFaviconStatus(favicon_status);
  NotifyFaviconUpdatedObservers(notification_icon_type, favicon_status.url,
                                icon_url_changed, favicon_status.image);
}

WEB_STATE_USER_DATA_KEY_IMPL(WebFaviconDriver)

}  // namespace favicon

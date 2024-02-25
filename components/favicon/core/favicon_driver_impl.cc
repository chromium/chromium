// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_driver_impl.h"

#include <memory>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/favicon/core/core_favicon_service.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/favicon/core/favicon_handler.h"
#include "components/favicon/core/favicon_url.h"

namespace favicon {
namespace {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
const bool kEnableTouchIcon = true;
#else
const bool kEnableTouchIcon = false;
#endif

}  // namespace

FaviconDriverImpl::FaviconDriverImpl(CoreFaviconService* favicon_service)
    : favicon_service_(favicon_service) {
  if (kEnableTouchIcon) {
    handlers_.push_back(std::make_unique<FaviconHandler>(
        favicon_service_, this, FaviconDriverObserver::NON_TOUCH_LARGEST));
    handlers_.push_back(std::make_unique<FaviconHandler>(
        favicon_service_, this, FaviconDriverObserver::TOUCH_LARGEST));
  } else {
    handlers_.push_back(std::make_unique<FaviconHandler>(
        favicon_service_, this, FaviconDriverObserver::NON_TOUCH_16_DIP));
  }
}

FaviconDriverImpl::~FaviconDriverImpl() = default;

void FaviconDriverImpl::FetchFavicon(const GURL& page_url,
                                     bool is_same_document) {
  for (const std::unique_ptr<FaviconHandler>& handler : handlers_)
    handler->FetchFavicon(page_url, is_same_document);
}

bool FaviconDriverImpl::HasPendingTasksForTest() {
  for (const std::unique_ptr<FaviconHandler>& handler : handlers_) {
    if (handler->HasPendingTasksForTest())
      return true;
  }
  return false;
}

void FaviconDriverImpl::SetFaviconOutOfDateForPage(const GURL& url,
                                                   bool force_reload) {
  if (favicon_service_) {
    favicon_service_->SetFaviconOutOfDateForPage(url);
    if (force_reload)
      favicon_service_->ClearUnableToDownloadFavicons();
  }
}

void FaviconDriverImpl::OnUpdateCandidates(
    const GURL& page_url,
    const std::vector<FaviconURL>& candidates,
    const GURL& manifest_url) {
  for (const std::unique_ptr<FaviconHandler>& handler : handlers_) {
    // We feed in the Web Manifest URL (if any) to the instance handling type
    // kWebManifestIcon, because those compete which each other (i.e. manifest
    // icons override inline touch icons).
    handler->OnUpdateCandidates(
        page_url, candidates,
        (handler->icon_types().count(
             favicon_base::IconType::kWebManifestIcon) != 0)
            ? manifest_url
            : GURL());
  }
}

}  // namespace favicon

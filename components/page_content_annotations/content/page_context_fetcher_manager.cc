// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_context_fetcher_manager.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace page_content_annotations {

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContextFetcherManager);

PageContextFetcherManager::PageContextFetcherManager(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PageContextFetcherManager>(*web_contents) {}

PageContextFetcherManager::~PageContextFetcherManager() = default;

void PageContextFetcherManager::Fetch(
    const FetchPageContextOptions& options,
    std::unique_ptr<FetchPageProgressListener> progress_listener,
    GetScreenshotServiceCallback get_screenshot_service_callback,
    FetchPageContextResultCallback callback) {
  CHECK(callback);
  auto fetcher = std::make_unique<PageContextFetcher>(
      std::move(get_screenshot_service_callback), std::move(progress_listener));

  PageContextFetcher* raw_fetcher = fetcher.get();
  active_fetchers_.emplace(raw_fetcher, std::move(fetcher));

  auto wrapped_callback = std::move(callback).Then(
      base::BindOnce(&PageContextFetcherManager::OnFetchComplete,
                     weak_ptr_factory_.GetWeakPtr(), raw_fetcher));

  raw_fetcher->FetchStart(GetWebContents(), options,
                          std::move(wrapped_callback));
}

void PageContextFetcherManager::OnFetchComplete(PageContextFetcher* fetcher) {
  size_t erased = active_fetchers_.erase(fetcher);
  CHECK_EQ(erased, 1u);
}

}  // namespace page_content_annotations

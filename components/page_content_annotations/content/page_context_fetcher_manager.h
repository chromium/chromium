// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_MANAGER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace page_content_annotations {

// Manages the lifetime of active PageContextFetcher instances.
class PageContextFetcherManager
    : public content::WebContentsUserData<PageContextFetcherManager> {
 public:
  ~PageContextFetcherManager() override;

  PageContextFetcherManager(const PageContextFetcherManager&) = delete;
  PageContextFetcherManager& operator=(const PageContextFetcherManager&) =
      delete;

  // Starts a page context fetch for the WebContents associated with `this`.
  // The fetcher will be owned by `this` and destroyed when the fetch completes
  // or when the WebContents is destroyed.
  void Fetch(const FetchPageContextOptions& options,
             std::unique_ptr<FetchPageProgressListener> progress_listener,
             GetScreenshotServiceCallback get_screenshot_service_callback,
             FetchPageContextResultCallback callback);

 private:
  friend class content::WebContentsUserData<PageContextFetcherManager>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit PageContextFetcherManager(content::WebContents* web_contents);

  void OnFetchComplete(PageContextFetcher* fetcher);

  base::flat_map<PageContextFetcher*, std::unique_ptr<PageContextFetcher>>
      active_fetchers_;
  base::WeakPtrFactory<PageContextFetcherManager> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_MANAGER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_HANDLER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/page_content_annotations/core/page_content_cache.h"
#include "components/page_content_annotations/core/page_content_store.h"
#include "components/page_content_annotations/core/web_state_wrapper.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class TimeDelta;
}  // namespace base

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

namespace page_content_annotations {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with PageContentExtractionAndCachingStatus in enums.xml.
// LINT.IfChange(PageContentExtractionAndCachingStatus)
enum class PageContentExtractionAndCachingStatus {
  kUnknown = 0,
  kExtractionObservedInForeground = 1,
  kExtractionObservedInBackground = 2,
  kContentsAvailableWhenBackgrounded = 3,
  kContentsNotAvailableWhenBackgrounded = 4,
  kContentsDeletedOnTabClose = 5,
  kContentsDeletedOnTabUpdate = 6,
  kContentsAvailableOnTabCloseUndone = 7,
  kMaxValue = kContentsAvailableOnTabCloseUndone,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/optimization/enums.xml:PageContentExtractionAndCachingStatus)

// Handles notifications from various observers to interact with the
// PageContentCache.
class PageContentCacheHandler {
 public:
  PageContentCacheHandler(os_crypt_async::OSCryptAsync* os_crypt_async,
                          const base::FilePath& profile_path,
                          base::TimeDelta max_context_age);
  ~PageContentCacheHandler();

  PageContentCacheHandler(const PageContentCacheHandler&) = delete;
  PageContentCacheHandler& operator=(const PageContentCacheHandler&) = delete;

  // Called when a tab is closed and the closure can still be undone.
  void OnTabClosed(int64_t tab_id);

  // Called when a tab's closure is undone.
  void OnTabCloseUndone(int64_t tab_id);

  // Called when a tab closure is committed and can't be undone anymore.
  void TabClosureCommitted(int64_t tab_id);

  // Called when the visibility of a WebContents changes.
  void OnVisibilityChanged(
      std::optional<int64_t> tab_id,
      const WebStateWrapper& web_state,
      std::optional<optimization_guide::proto::PageContext> page_context,
      const base::Time& extraction_time);

  // Called when a new navigation happens in a WebContents.
  void OnNewNavigation(std::optional<int64_t> tab_id,
                       const WebStateWrapper& web_state);

  void ProcessPageContentExtraction(
      std::optional<int64_t> tab_id,
      const WebStateWrapper& web_state,
      const optimization_guide::proto::PageContext& page_context,
      const base::Time& extraction_time);

  PageContentCache* page_content_cache() { return page_content_cache_.get(); }

 private:
  friend class PageContentCacheHandlerTest;

  // Called when the content for a closed tab has been retrieved from the cache.
  void OnPageContentRetrievedForClosedTab(
      int64_t tab_id,
      std::optional<optimization_guide::PageContentResult> result);

  // Returns whether the tab with `tab_id` is currently considered closed.
  bool IsTabClosed(int64_t tab_id) const;

  // Updates the cache with the given page content if the tab is not closed. If
  // the tab is closed, remove it from the closed tabs map.
  void UpdateCache(int64_t tab_id,
                   const GURL& url,
                   const base::Time& navigation_timestamp,
                   const base::Time& extraction_time,
                   const optimization_guide::proto::PageContext& page_context);

  const std::unique_ptr<PageContentCache> page_content_cache_;

  // The map of (tab IDs, PageContentResult) that are currently considered
  // closed, but have not yet had their closure committed (i.e the tab closure
  // can still be undone).
  absl::flat_hash_map<int64_t,
                      std::unique_ptr<optimization_guide::PageContentResult>>
      closed_tabs_;

  // The set of tab IDs that have had their closure committed (the tab closure
  // can no longer be undone).
  absl::flat_hash_set<int64_t> committed_closed_tabs_;

  base::WeakPtrFactory<PageContentCacheHandler> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_HANDLER_H_

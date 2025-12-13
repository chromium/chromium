// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_HANDLER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/scoped_observation.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/core/web_state_wrapper.h"

namespace base {
class FilePath;
class TimeDelta;
}  // namespace base

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

namespace page_content_annotations {

class PageContentCache;

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

  // Called when a tab is closed.
  void OnTabClosed(int64_t tab_id);

  // Called when a closed tab is undone.
  void OnTabCloseUndone(int64_t tab_id);

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
  // Returns whether the tab with `tab_id` is currently considered closed.
  bool IsTabClosed(int64_t tab_id) const;

  const std::unique_ptr<PageContentCache> page_content_cache_;

  // The set of tab IDs that are currently considered closed.
  std::set<int64_t> closed_tabs_;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_CACHE_HANDLER_H_

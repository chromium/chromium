// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_cache_handler.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_cache.h"
#include "components/page_content_annotations/core/web_state_wrapper.h"

namespace page_content_annotations {

namespace {

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
  kMaxValue = kContentsDeletedOnTabUpdate,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/optimization/enums.xml:PageContentExtractionAndCachingStatus)

void RecordExtractionAndCachingStatus(
    PageContentExtractionAndCachingStatus status) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus", status);
}

}  // namespace

PageContentCacheHandler::PageContentCacheHandler(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    const base::FilePath& profile_path)
    : page_content_cache_(
          std::make_unique<PageContentCache>(os_crypt_async, profile_path)) {}

PageContentCacheHandler::~PageContentCacheHandler() = default;

void PageContentCacheHandler::OnTabClosed(int64_t tab_id) {
  RecordExtractionAndCachingStatus(
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabClose);
  page_content_cache_->RemovePageContentForTab(tab_id);
}

void PageContentCacheHandler::OnVisibilityChanged(
    std::optional<int64_t> tab_id,
    const WebStateWrapper& web_state,
    std::optional<optimization_guide::proto::PageContext> page_context) {
  if (!tab_id || web_state.is_off_the_record) {
    return;
  }
  if (web_state.visibility != PageContentVisibility::kHidden) {
    return;
  }
  if (!page_context) {
    RecordExtractionAndCachingStatus(PageContentExtractionAndCachingStatus::
                                         kContentsNotAvailableWhenBackgrounded);
    return;
  }
  // Even if background trigger is enabled, update the cache with available page
  // contents. This is to avoid losing context if tab was killed as soon as it
  // was hidden. If extraction succeeds, then cache would be updated again in
  // ProcessPageContentExtraction().

  // TODO(crbug.com/440643544): Pass in the extraction timestamp.
  page_content_cache_->CachePageContent(*tab_id, web_state.last_committed_url,
                                        web_state.navigation_timestamp,
                                        base::Time::Now(), *page_context);
  RecordExtractionAndCachingStatus(PageContentExtractionAndCachingStatus::
                                       kContentsAvailableWhenBackgrounded);
}

void PageContentCacheHandler::OnNewNavigation(
    std::optional<int64_t> tab_id,
    const WebStateWrapper& web_state) {
  if (!tab_id || web_state.is_off_the_record) {
    return;
  }
  RecordExtractionAndCachingStatus(
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabUpdate);
  // Delete cached contents for the tab_id when page is updated.
  page_content_cache_->RemovePageContentForTab(*tab_id);
}

void PageContentCacheHandler::ProcessPageContentExtraction(
    std::optional<int64_t> tab_id,
    const WebStateWrapper& web_state,
    const optimization_guide::proto::PageContext& page_context) {
  if (!tab_id || web_state.is_off_the_record) {
    return;
  }

  // This method only handles the case when extraction finishes when tab is
  // already backgrounded. We do not cache contents for active tab since it can
  // be extracted on demand.
  if (web_state.visibility == PageContentVisibility::kHidden) {
    RecordExtractionAndCachingStatus(
        PageContentExtractionAndCachingStatus::kExtractionObservedInBackground);
    page_content_cache_->CachePageContent(*tab_id, web_state.last_committed_url,
                                          web_state.navigation_timestamp,
                                          base::Time::Now(), page_context);
  } else {
    RecordExtractionAndCachingStatus(
        PageContentExtractionAndCachingStatus::kExtractionObservedInForeground);
  }
}

}  // namespace page_content_annotations

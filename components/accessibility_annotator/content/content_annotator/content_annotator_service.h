// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_

#include "base/containers/lru_cache.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "url/gurl.h"

namespace translate {
struct LanguageDetectionDetails;
}  // namespace translate

namespace accessibility_annotator {

class ContentAnnotatorService
    : public KeyedService,
      public page_content_annotations::PageContentAnnotationsService::
          PageContentAnnotationsObserver {
 public:
  explicit ContentAnnotatorService(
      page_content_annotations::PageContentAnnotationsService&
          page_content_annotations_service);
  ~ContentAnnotatorService() override;

  ContentAnnotatorService(const ContentAnnotatorService&) = delete;
  ContentAnnotatorService& operator=(const ContentAnnotatorService&) = delete;

  // page_content_annotations::PageContentAnnotationsService::
  //     PageContentAnnotationsObserver:
  void OnPageContentAnnotated(
      const page_content_annotations::HistoryVisit& visit,
      const page_content_annotations::PageContentAnnotationsResult& result)
      override;

  // Called when the language of the contents of the current page has been
  // determined. This will be called by the ContentAnnotatorTabHelper, which
  // observes the translate::TranslateDriver.
  // Virtual for testing.
  virtual void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details);

 private:
  using CacheIterator =
      base::LRUCache<GURL, ContentClassificationInput>::iterator;

  // Returns the iterator to the entry for the given URL, creating a new entry
  // if it does not exist.
  CacheIterator GetOrCreateJoinEntry(const GURL& url);

  // If the data is complete, runs content classification and determines
  // annotation eligibility.
  void MaybeAnnotate(CacheIterator it);

  const raw_ref<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_;

  // Stores and joins data for URLs that are pending annotation. The cache size
  // is `kContentAnnotatorMaxPendingUrls`. When the cache is full, the last
  // modified entry is evicted. The cache should only go over capacity in the
  // case expected data from an observation does not arrive or the user
  // navigates to several URLs faster than data can be collected for the URL and
  // annotations processed.
  base::LRUCache<GURL, ContentClassificationInput> join_entries_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_

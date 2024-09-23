// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_SENSITIVITY_CACHE_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_SENSITIVITY_CACHE_H_

#include "chrome/browser/profiles/profile.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "url/gurl.h"

// In order to have GURL as a key in a hashmap, GURL hashing mechanism is
// needed.
struct GURLHash {
  size_t operator()(const GURL& url) const {
    return std::hash<std::string>()(url.spec());
  }
};

// Adapts the sensitivity scores from the PageContentAnnotationService into a
// synchronously available form, for specifically the currently open tabs, by
// caching scores emitted to PageContentAnnotationsObservers.
class TabSensitivityCache final
    : public page_content_annotations::PageContentAnnotationsService::
          PageContentAnnotationsObserver {
 public:
  explicit TabSensitivityCache(Profile* profile);
  ~TabSensitivityCache() override;

  // Returns the sensitivity score for `url`, if it currently has one. The score
  // is a float from 0 to 1, where 1 is most likely to be sensitive.
  std::optional<float> GetScore(const GURL& url) const;

  // page_content_annotations
  //     ::PageContentAnnotationsService
  //     ::PageContentAnnotationsObserver
  void OnPageContentAnnotated(
      const GURL& url,
      const page_content_annotations::PageContentAnnotationsResult& result)
      override;

 private:
  void MaybeTrimCacheKeys();

  raw_ptr<Profile> profile_;
  std::unordered_map<GURL, float, GURLHash> sensitivy_scores_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_SENSITIVITY_CACHE_H_

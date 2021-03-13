// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_H_

#include <vector>

// A structure containing the annotations made to page content.
class PageContentAnnotations {
 public:
  PageContentAnnotations(const std::vector<std::pair<int, float>>& categories,
                         float floc_protected_score);
  ~PageContentAnnotations();
  PageContentAnnotations(const PageContentAnnotations& other);

  const std::vector<std::pair<int, float>>& categories() const {
    return categories_;
  }

  float floc_protected_score() const { return floc_protected_score_; }

 private:
  // A vector that contains category IDs and their weights.
  std::vector<std::pair<int, float>> categories_;
  // A value from 0 to 1 that represents whether the page content is
  // FLoC-protected.
  float floc_protected_score_;
};

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_H_

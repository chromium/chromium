// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_H_

#include <map>

// A structure containing the annotations made to page content.
class PageContentAnnotations {
 public:
  PageContentAnnotations(const std::map<int, float>& categories,
                         float sensitivity);
  ~PageContentAnnotations();
  PageContentAnnotations(const PageContentAnnotations& other);

  const std::map<int, float>& categories() const { return categories_; }

  float sensitivity() const { return sensitivity_; }

 private:
  // A mapping from category ID to weight.
  std::map<int, float> categories_;
  // A value from 0 to 1 that represents how sensitive the content is.
  float sensitivity_;
};

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_H_

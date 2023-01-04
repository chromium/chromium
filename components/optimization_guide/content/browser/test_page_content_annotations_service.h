// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_TEST_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_TEST_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_

#include "base/files/scoped_temp_dir.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"

namespace optimization_guide {

// A testing version of |PageContentAnnotationsService| for use in tests.
class TestPageContentAnnotationsService : public PageContentAnnotationsService {
 public:
  // Creates a new instance of |TestPageContentAnnotationsService|, always
  // returning non-null.
  //
  // The model provider and history service are both optional. nullptr can be
  // passed to use their for-test versions.
  static std::unique_ptr<TestPageContentAnnotationsService> Create(
      OptimizationGuideModelProvider* optimization_guide_model_provider,
      history::HistoryService* history_service);

  ~TestPageContentAnnotationsService() override;
  TestPageContentAnnotationsService(const TestPageContentAnnotationsService&) =
      delete;
  TestPageContentAnnotationsService& operator=(
      const TestPageContentAnnotationsService&) = delete;

 private:
  TestPageContentAnnotationsService(
      OptimizationGuideModelProvider* optimization_guide_model_provider,
      history::HistoryService* history_service);

  std::unique_ptr<base::ScopedTempDir> temp_dir_;
  std::unique_ptr<TestOptimizationGuideModelProvider> test_model_provider_;
  std::unique_ptr<history::HistoryService> test_history_service_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_TEST_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_

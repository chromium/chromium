// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_TEST_PAGE_CONTENT_ANNOTATOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_TEST_PAGE_CONTENT_ANNOTATOR_H_

#include "base/containers/flat_map.h"
#include "components/optimization_guide/content/browser/page_content_annotator.h"
#include "components/optimization_guide/core/page_content_annotations_common.h"

namespace optimization_guide {

// Pass this to
// |PageContentAnnotationsService::OverridePageContentAnnotatorForTesting| for
// unit testing, or just build this into the prod binary for local development
// manual testing and hard code the desired output.
class TestPageContentAnnotator : public PageContentAnnotator {
 public:
  TestPageContentAnnotator();
  ~TestPageContentAnnotator() override;

  // The given page topics are used for the matching BatchAnnotationResults by
  // input string. If the input is not found, the output is left as nullopt.
  void UsePageTopics(
      const base::flat_map<std::string, std::vector<WeightedIdentifier>>&
          topics_by_input);

  // The given page entities are used for the matching BatchAnnotationResults by
  // input string. If the input is not found, the output is left as nullopt.
  void UsePageEntities(
      const base::flat_map<std::string, std::vector<ScoredEntityMetadata>>&
          entities_by_input);

  // The given visibility score is used for the matching BatchAnnotationResults
  // by input string. If the input is not found, the output is left as nullopt.
  void UseVisibilityScores(
      const base::flat_map<std::string, double>& visibility_scores_for_input);

  // PageContentAnnotator:
  void Annotate(BatchAnnotationCallback callback,
                const std::vector<std::string>& inputs,
                AnnotationType annotation_type) override;

 private:
  base::flat_map<std::string, std::vector<WeightedIdentifier>> topics_by_input_;
  base::flat_map<std::string, std::vector<ScoredEntityMetadata>>
      entities_by_input_;
  base::flat_map<std::string, double> visibility_scores_for_input_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_TEST_PAGE_CONTENT_ANNOTATOR_H_

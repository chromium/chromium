// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TEST_PAGE_CONTENT_ANNOTATOR_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TEST_PAGE_CONTENT_ANNOTATOR_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/page_content_annotations/core/page_content_annotator.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"

namespace page_content_annotations {

// Pass this to
// |PageContentAnnotationsService::OverridePageContentAnnotatorForTesting| for
// unit testing, or just build this into the prod binary for local development
// manual testing and hard code the desired output.
class TestPageContentAnnotator : public PageContentAnnotator {
 public:
  TestPageContentAnnotator();
  ~TestPageContentAnnotator() override;

  // The given visibility score is used for the matching BatchAnnotationResults
  // by input string. If the input is not found, the output is left as nullopt.
  void UseVisibilityScores(
      const std::optional<optimization_guide::ModelInfo>& model_info,
      const base::flat_map<std::string, double>& visibility_scores_for_input);

  // When set, |Annotate| will never call its callback.
  void SetAlwaysHang(bool hang);

  // Returns true iff |RequestAndNotifyWhenModelAvailable| was called for
  // |type|.
  bool ModelRequestedForType(AnnotationType type) const;

  using AnnotateInputsAndType =
      std::pair<std::vector<std::string>, AnnotationType>;
  const std::vector<AnnotateInputsAndType>& annotation_requests() const {
    return annotation_requests_;
  }

  // PageContentAnnotator:
  void Annotate(BatchAnnotationCallback callback,
                const std::vector<std::string>& inputs,
                AnnotationType annotation_type) override;
  std::optional<optimization_guide::ModelInfo> GetModelInfoForType(
      AnnotationType annotation_type) const override;
  void RequestAndNotifyWhenModelAvailable(
      AnnotationType type,
      base::OnceCallback<void(bool)> callback) override;

 private:
  // When set, |Annotate| will never call its callback.
  bool always_hang_ = false;

  std::optional<optimization_guide::ModelInfo> visibility_scores_model_info_;
  base::flat_map<std::string, double> visibility_scores_for_input_;

  std::vector<AnnotateInputsAndType> annotation_requests_;

  base::flat_set<AnnotationType> model_requests_;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TEST_PAGE_CONTENT_ANNOTATOR_H_

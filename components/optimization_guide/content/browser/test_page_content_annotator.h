// Copyright 2021 The Chromium Authors
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

  // The given page entities are used for the matching BatchAnnotationResults by
  // input string. If the input is not found, the output is left as nullopt.
  void UsePageEntities(
      const absl::optional<ModelInfo>& model_info,
      const base::flat_map<std::string, std::vector<ScoredEntityMetadata>>&
          entities_by_input);

  // The given visibility score is used for the matching BatchAnnotationResults
  // by input string. If the input is not found, the output is left as nullopt.
  void UseVisibilityScores(
      const absl::optional<ModelInfo>& model_info,
      const base::flat_map<std::string, double>& visibility_scores_for_input);

  // The given text embedding is used for the matching BatchAnnotationResults
  // by input string. If the input is not found, the output is left as nullopt.
  void UseTextEmbeddings(const absl::optional<ModelInfo>& model_info,
                         const base::flat_map<std::string, std::vector<float>>&
                             text_embeddings_for_input);

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
  absl::optional<ModelInfo> GetModelInfoForType(
      AnnotationType annotation_type) const override;
  void RequestAndNotifyWhenModelAvailable(
      AnnotationType type,
      base::OnceCallback<void(bool)> callback) override;

 private:
  // When set, |Annotate| will never call its callback.
  bool always_hang_ = false;

  absl::optional<ModelInfo> entities_model_info_;
  base::flat_map<std::string, std::vector<ScoredEntityMetadata>>
      entities_by_input_;

  absl::optional<ModelInfo> visibility_scores_model_info_;
  base::flat_map<std::string, double> visibility_scores_for_input_;

  absl::optional<ModelInfo> text_embeddings_model_info_;
  base::flat_map<std::string, std::vector<float>> text_embeddings_for_input_;

  std::vector<AnnotateInputsAndType> annotation_requests_;

  base::flat_set<AnnotationType> model_requests_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_TEST_PAGE_CONTENT_ANNOTATOR_H_

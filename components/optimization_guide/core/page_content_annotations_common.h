// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATIONS_COMMON_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATIONS_COMMON_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "components/optimization_guide/core/page_content_annotation_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace optimization_guide {

// A weighted ID value.
class WeightedIdentifier {
 public:
  WeightedIdentifier(int32_t value, double weight);
  WeightedIdentifier(const WeightedIdentifier&);
  ~WeightedIdentifier();

  int32_t value() const { return value_; }
  double weight() const { return weight_; }

  std::string ToString() const;

  base::Value AsValue() const;

  bool operator==(const WeightedIdentifier& other) const;

  friend std::ostream& operator<<(std::ostream& stream,
                                  const WeightedIdentifier& ws);

 private:
  int32_t value_;

  // In the range of [0.0, 1.0].
  double weight_ = 0;
};

// The result of an execution, and all associated data.
class BatchAnnotationResult {
 public:
  // Creates a result for a page entities annotation.
  static BatchAnnotationResult CreatePageEntitiesResult(
      const std::string& input,
      absl::optional<std::vector<ScoredEntityMetadata>> entities);

  // Creates a result for a content visibility annotation.
  static BatchAnnotationResult CreateContentVisibilityResult(
      const std::string& input,
      absl::optional<double> visibility_score);

  // Creates a result for a text embedding annotation.
  static BatchAnnotationResult CreateTextEmbeddingResult(
      const std::string& input,
      absl::optional<std::vector<float>> embeddings);

  // Creates a result where the AnnotationType and output are not set.
  static BatchAnnotationResult CreateEmptyAnnotationsResult(
      const std::string& input);

  BatchAnnotationResult(const BatchAnnotationResult&);
  ~BatchAnnotationResult();

  // Returns true if the output corresponding to |type| is not nullopt;
  bool HasOutputForType() const;

  const std::string& input() const { return input_; }
  AnnotationType type() const { return type_; }
  const absl::optional<std::vector<ScoredEntityMetadata>>& entities() const {
    return entities_;
  }
  absl::optional<double> visibility_score() const { return visibility_score_; }
  absl::optional<std::vector<float>> embeddings() const { return embeddings_; }

  std::string ToString() const;
  std::string ToJSON() const;

  base::Value AsValue() const;

  bool operator==(const BatchAnnotationResult& other) const;

  friend std::ostream& operator<<(std::ostream& stream,
                                  const BatchAnnotationResult& result);

 private:
  BatchAnnotationResult();

  std::string input_;
  AnnotationType type_ = AnnotationType::kUnknown;

  // Output for page entities annotations, set only if the |type_| matches and
  // the execution was successful.
  absl::optional<std::vector<ScoredEntityMetadata>> entities_;

  // Output for visisbility score annotations, set only if the |type_| matches
  // and the execution was successful.
  absl::optional<double> visibility_score_;

  // Output for text emebdding annotations, set only if the |type_| matches
  // and the execution was successful.
  absl::optional<std::vector<float>> embeddings_;
};

using BatchAnnotationCallback =
    base::OnceCallback<void(const std::vector<BatchAnnotationResult>&)>;

// Creates a vector of |BatchAnnotationResult| from the given |inputs| where
// each result's status is set to |status|. Useful for creating an Annotation
// response with a single error.
std::vector<BatchAnnotationResult> CreateEmptyBatchAnnotationResults(
    const std::vector<std::string>& inputs);

// The result of various types of PageContentAnnotation.
class PageContentAnnotationsResult {
  // The various type of results.
  typedef float ContentVisibilityScore;

 public:
  // Creates a result for a content visibility annotation.
  static PageContentAnnotationsResult CreateContentVisibilityScoreResult(
      const ContentVisibilityScore& score);

  PageContentAnnotationsResult(const PageContentAnnotationsResult&);
  PageContentAnnotationsResult& operator=(const PageContentAnnotationsResult&);
  ~PageContentAnnotationsResult();

  // Returns the type of annotation in this result.
  AnnotationType GetType() const;

  ContentVisibilityScore GetContentVisibilityScore() const;

 private:
  PageContentAnnotationsResult();

  // The page content annotation of this result.
  absl::variant<void* /*Unknown*/, ContentVisibilityScore> result_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATIONS_COMMON_H_

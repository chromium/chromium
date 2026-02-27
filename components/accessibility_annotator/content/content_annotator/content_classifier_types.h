// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_TYPES_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_TYPES_H_

#include <optional>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace accessibility_annotator {

// The possible types of dependent information that might be missing from a
// page that is undergoing evaluation for annotation. Used for logging.
// LINT.IfChange(ContentAnnotatorMissingDependentInformation)
enum class ContentAnnotatorMissingDependentInformation {
  kSensitivityScoreMissing = 0,
  kNavigationTimestampMissing = 1,
  kAdoptedLanguageMissing = 2,
  kPageTitleMissing = 3,
  kAnnotatedPageContentMissing = 4,
  kMaxValue = kAnnotatedPageContentMissing,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility_annotator/enums.xml:ContentAnnotatorDependentInformationTypes)

// The input to the content classifier, containing all data that might be used
// for classification.
struct ContentClassificationInput {
  explicit ContentClassificationInput(GURL url);
  ContentClassificationInput(const ContentClassificationInput&);
  ContentClassificationInput& operator=(const ContentClassificationInput&);
  ContentClassificationInput(ContentClassificationInput&&);
  ContentClassificationInput& operator=(ContentClassificationInput&&);
  ~ContentClassificationInput();

  GURL url;
  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  // LINT.IfChange
  std::optional<float> sensitivity_score;
  std::optional<base::Time> navigation_timestamp;
  std::optional<std::string> adopted_language;
  std::optional<std::string> page_title;
  scoped_refptr<const base::RefCountedData<
      optimization_guide::proto::AnnotatedPageContent>>
      annotated_page_content;
  // LINT.ThenChange(//components/accessibility_annotator/content/content_annotator/content_classifier_types.cc:ContentClassificationInputIsComplete)

  // Returns true if all fields are populated.
  bool IsComplete() const;

  // Logs all missing fields to the missing dependencies UMA histogram.
  void LogMissingFields() const;
};

// The result of a content classification, containing the output of one or more
// individual classifiers.
struct ContentClassificationResult {
  ContentClassificationResult();
  ContentClassificationResult(const ContentClassificationResult&);
  ContentClassificationResult& operator=(const ContentClassificationResult&);
  ContentClassificationResult(ContentClassificationResult&&);
  ContentClassificationResult& operator=(ContentClassificationResult&&);
  ~ContentClassificationResult();

  struct Result {
    // TODO(crbug.com/485267512): Consider making separate Result structs
    // for different classifier types.
    Result();
    Result(const Result&);
    Result& operator=(const Result&);
    Result(Result&&);
    Result& operator=(Result&&);
    ~Result();

    std::optional<std::string> category;
  };

  std::optional<Result> title_keyword_result;
  std::optional<Result> url_match_result;
  std::optional<bool> is_sensitive;
  std::optional<bool> is_in_target_language;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_CLASSIFIER_TYPES_H_

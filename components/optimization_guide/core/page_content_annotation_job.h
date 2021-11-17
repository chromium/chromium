// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATION_JOB_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATION_JOB_H_

#include <deque>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/optimization_guide/core/page_content_annotations_common.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// A single page content annotation job with all request and response data
// throughout the progression of the model execution. It can contain one or more
// inputs to be annotated, specified by the AnnotationType. This is a data
// container that matches the I/O of a single call to the PCA Service.
class PageContentAnnotationJob {
 public:
  using WeightedCategories = std::vector<WeightedString>;

  PageContentAnnotationJob(BatchAnnotationCallback on_complete_callback,
                           const std::vector<std::string>& inputs,
                           AnnotationType type);
  ~PageContentAnnotationJob();

  // Consumes every input, posting new results with nullopt outputs.
  void FillWithNullOutputs();

  // Called when the Job has finished executing to call |on_complete_callback_|.
  void OnComplete();

  // Returns the next input to be annotated, effectively "draining" the
  // |inputs_| queue. Guaranteed to be non-null for the next
  // |CountOfRemainingNonNullInputs| number of calls.
  absl::optional<std::string> GetNextInput();

  // The count of remaining inputs. |GetNextInput| can be called this many times
  // without return nullopt.
  size_t CountOfRemainingNonNullInputs() const;

  // Posts a new result after an execution has completed.
  void PostNewResult(const BatchAnnotationResult& result);

  AnnotationType type() const { return type_; }

  PageContentAnnotationJob(const PageContentAnnotationJob&) = delete;
  PageContentAnnotationJob& operator=(const PageContentAnnotationJob&) = delete;

 private:
  // Run with |results_| when |OnComplete()| is called.
  BatchAnnotationCallback on_complete_callback_;

  // The requested annotation type.
  const AnnotationType type_;

  // This is filled with all of the passed inputs in the ctor, then slowly
  // drained from the beginning to end by |GetNextInput|.
  std::deque<std::string> inputs_;

  // Filled by |PostNewResult| with the complete annotations, specified by
  // |type_|.
  std::vector<BatchAnnotationResult> results_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATION_JOB_H_
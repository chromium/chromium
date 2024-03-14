// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATION_JOB_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATION_JOB_H_

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"

namespace page_content_annotations {

// A single page content annotation job with all request and response data
// throughout the progression of the model execution. It can contain one or more
// inputs to be annotated, specified by the AnnotationType. This is a data
// container that matches the I/O of a single call to the PCA Service.
class PageContentAnnotationJob {
 public:
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
  std::optional<std::string> GetNextInput();

  // The count of remaining inputs. |GetNextInput| can be called this many times
  // without return nullopt.
  size_t CountOfRemainingNonNullInputs() const;

  // Posts a new result after an execution has completed for the given input
  // |index|.
  void PostNewResult(const BatchAnnotationResult& result, size_t index);

  // Returns true if any element of |results_| was a successful execution. We
  // expect that if one result is successful, many more will be as well.
  bool HadAnySuccess() const;

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

  // The time the job was constructed.
  const base::TimeTicks job_creation_time_;

  // Set when |GetNextInput| is called for the first time.
  std::optional<base::TimeTicks> job_execution_start_time_;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATION_JOB_H_
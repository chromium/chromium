// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATION_JOB_EXECUTOR_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATION_JOB_EXECUTOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/page_content_annotations/core/page_content_annotation_job.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/category.h"

namespace page_content_annotations {

// An abstract class that serves as an adapter between the multiple string
// inputs of a PageContentAnnotationJob and the actual model execution which
// works on a single string input at a time.
class PageContentAnnotationJobExecutor {
 public:
  PageContentAnnotationJobExecutor();
  ~PageContentAnnotationJobExecutor();

  // Ownership of |job| passes to |this|, and |this| will take care of
  // executing the job and calling its OnComplete method, then running the
  // |on_job_complete_callback| to notify the caller that the job is
  // complete.
  // Virtual to allow derived classes to override the default behavior, though
  // they should still call the base implementation eventually.
  virtual void ExecuteJob(base::OnceClosure on_job_complete_callback,
                          std::unique_ptr<PageContentAnnotationJob> job);

 protected:
  // Implemented by derived classes to execute a model input.
  virtual void ExecuteOnSingleInput(
      AnnotationType annotation_type,
      const std::string& input,
      base::OnceCallback<void(const BatchAnnotationResult&)> callback) = 0;

 private:
  // Called when the |job| finishes executing.
  void OnJobExecutionComplete(
      base::OnceClosure on_job_complete_callback_from_caller,
      std::unique_ptr<PageContentAnnotationJob> job);

  // Called when a single page entities input has finished executing for |job|.
  // |on_single_input_done_barrier_closure| is a base::BarrierClosure that, once
  // all inputs in the job have completed, will run |OnJobExecutionComplete| and
  // then destroy |job|. |input| is the original input of the model execution.
  void OnSingleInputExecutionComplete(
      PageContentAnnotationJob* job,
      size_t index,
      base::OnceClosure on_single_input_done_barrier_closure,
      const BatchAnnotationResult& output);

  base::WeakPtrFactory<PageContentAnnotationJobExecutor> weak_ptr_factory_{
      this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATION_JOB_EXECUTOR_H_

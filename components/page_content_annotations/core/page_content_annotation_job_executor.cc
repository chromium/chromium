// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotation_job_executor.h"

#include "base/barrier_closure.h"
#include "base/check_op.h"

namespace page_content_annotations {

PageContentAnnotationJobExecutor::PageContentAnnotationJobExecutor() = default;
PageContentAnnotationJobExecutor::~PageContentAnnotationJobExecutor() = default;

void PageContentAnnotationJobExecutor::ExecuteJob(
    base::OnceClosure on_job_complete_callback_from_caller,
    std::unique_ptr<PageContentAnnotationJob> unique_job) {
  PageContentAnnotationJob* job = unique_job.get();
  DCHECK(job);

  base::OnceClosure my_on_job_complete_callback = base::BindOnce(
      &PageContentAnnotationJobExecutor::OnJobExecutionComplete,
      weak_ptr_factory_.GetWeakPtr(),
      std::move(on_job_complete_callback_from_caller), std::move(unique_job));
  base::RepeatingClosure on_each_done_callback =
      base::BarrierClosure(job->CountOfRemainingNonNullInputs(),
                           std::move(my_on_job_complete_callback));
  //////// Notes on why the code is architected like this:
  // A job contains many inputs which each need to be executed. A single
  // execution can take up to 1 second so that latency is multiplied by the size
  // of the job. While it is important for jobs to execute and finish decently
  // quickly, it is also important to be good stewards of the background message
  // loop which is using the same hardware thread as other operations in Chrome.
  //
  // Running all the inputs in a single background message pump could hang the
  // underlying background thread for quite a while. However, if we only send
  // one input for execution at a time then completion of the job is subject to
  // the latency from other tasks in Chrome. Both of those approaches would
  // allow us to only use the unique_ptr by passing ownership of the job between
  // threads, reducing the complexity of the code below.
  //
  // But because of the potential latency regression on either other Chrome
  // tasks or our job, we've taken the approach of making many task scheduling
  // requests at once. This will let the background task runner to figure the
  // best way to schedule everything sooner. It comes at the cost of a more
  // complex lifetime of the job, which is detailed below.

  //////// Notes on lifetimes:
  // As of this point, |on_each_done_callback| owns |on_complete_callback|
  // which owns the underlying |job| instance.
  //
  // Running |on_each_done_callback| (especially when the input is empty, see
  // below) may destroy |job|. That makes it tricky for the loop to check
  // whether to continue since we need to use some state that is not tied to the
  // lifetime of |job|. Therefore, as pretty as
  // `while (auto input = job->GetNextInput())` would be, a more traditional for
  // loop is used using simple counting. In the same way,
  // `for ( ; i < job->CountOfRemainingNonNullInputs(); )` doesn't work either,
  // since |job| will be destroyed when the loop does its last exit check.
  const size_t num_inputs = job->CountOfRemainingNonNullInputs();
  for (size_t i = 0; i < num_inputs; i++) {
    std::string input = *job->GetNextInput();
    ExecuteOnSingleInput(
        job->type(), input,
        base::BindOnce(
            &PageContentAnnotationJobExecutor::OnSingleInputExecutionComplete,
            weak_ptr_factory_.GetWeakPtr(), job, i, on_each_done_callback));
  }
}

void PageContentAnnotationJobExecutor::OnJobExecutionComplete(
    base::OnceClosure on_job_complete_callback_from_caller,
    std::unique_ptr<PageContentAnnotationJob> job) {
  job->OnComplete();
  // Intentionally reset |job| here to make lifetime clearer and less bug-prone.
  // Note that the job dtor also records some timing metrics which is better to
  // do now rather than after the following callback.
  job.reset();

  std::move(on_job_complete_callback_from_caller).Run();
}

void PageContentAnnotationJobExecutor::OnSingleInputExecutionComplete(
    PageContentAnnotationJob* job,
    size_t index,
    base::OnceClosure on_single_input_done_barrier_closure,
    const BatchAnnotationResult& output) {
  job->PostNewResult(output, index);

  // Running |on_single_input_done_barrier_closure| may destroy |job|.
  std::move(on_single_input_done_barrier_closure).Run();
}

}  // namespace page_content_annotations

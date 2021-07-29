// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/copy_output_request.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace viz {

CopyOutputRequest::CopyOutputRequest(ResultFormat result_format,
                                     ResultDestination result_destination,
                                     CopyOutputRequestCallback result_callback)
    : result_format_(result_format),
      result_destination_(result_destination),
      result_callback_(std::move(result_callback)),
      scale_from_(1, 1),
      scale_to_(1, 1) {
  // If format is I420_PLANES, the result must be in system memory. Returning
  // I420_PLANES via textures is currently not supported.
  DCHECK(result_format != ResultFormat::I420_PLANES ||
         result_destination == ResultDestination::kSystemMemory);

  DCHECK(!result_callback_.is_null());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("viz", "CopyOutputRequest", this);
}

CopyOutputRequest::~CopyOutputRequest() {
  if (!result_callback_.is_null()) {
    // Send an empty result to indicate the request was never satisfied.
    SendResult(std::make_unique<CopyOutputResult>(
        result_format_, result_destination_, gfx::Rect(), false));
  }
}

void CopyOutputRequest::SetScaleRatio(const gfx::Vector2d& scale_from,
                                      const gfx::Vector2d& scale_to) {
  // These are CHECKs, and not DCHECKs, because it's critical that crash report
  // bugs be tied to the client callpoint rather than the later mojo or service-
  // side processing of the CopyOutputRequest.
  CHECK_GT(scale_from.x(), 0);
  CHECK_GT(scale_from.y(), 0);
  CHECK_GT(scale_to.x(), 0);
  CHECK_GT(scale_to.y(), 0);

  scale_from_ = scale_from;
  scale_to_ = scale_to;
}

void CopyOutputRequest::SetUniformScaleRatio(int scale_from, int scale_to) {
  // See note in SetScaleRatio() as to why these are CHECKs and not DCHECKs.
  CHECK_GT(scale_from, 0);
  CHECK_GT(scale_to, 0);

  scale_from_ = gfx::Vector2d(scale_from, scale_from);
  scale_to_ = gfx::Vector2d(scale_to, scale_to);
}

void CopyOutputRequest::SendResult(std::unique_ptr<CopyOutputResult> result) {
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "viz", "CopyOutputRequest", this, "success", !result->IsEmpty(),
      "has_provided_task_runner", !!result_task_runner_);
  // Serializing the result requires an expensive copy, so to not block the
  // any important thread we PostTask onto the threadpool by default, but if the
  // user has provided a task runner use that instead.
  auto runner =
      result_task_runner_
          ? result_task_runner_
          : base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  runner->PostTask(FROM_HERE, base::BindOnce(std::move(result_callback_),
                                             std::move(result)));
  // Remove the reference to the task runner (no-op if we didn't have one).
  result_task_runner_ = nullptr;
}

bool CopyOutputRequest::SendsResultsInCurrentSequence() const {
  return result_task_runner_ &&
         result_task_runner_->RunsTasksInCurrentSequence();
}

// static
std::unique_ptr<CopyOutputRequest> CopyOutputRequest::CreateStubForTesting() {
  return std::make_unique<CopyOutputRequest>(
      ResultFormat::RGBA, ResultDestination::kSystemMemory,
      base::BindOnce([](std::unique_ptr<CopyOutputResult>) {}));
}

}  // namespace viz

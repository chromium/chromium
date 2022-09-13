// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/copy_output_request.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

const char* ResultFormatToShortString(
    viz::CopyOutputRequest::ResultFormat result_format) {
  switch (result_format) {
    case viz::CopyOutputRequest::ResultFormat::RGBA:
      return "RGBA";
    case viz::CopyOutputRequest::ResultFormat::I420_PLANES:
      return "I420";
    case viz::CopyOutputRequest::ResultFormat::NV12_PLANES:
      return "NV12";
  }
}

const char* ResultDestinationToShortString(
    viz::CopyOutputRequest::ResultDestination result_destination) {
  switch (result_destination) {
    case viz::CopyOutputRequest::ResultDestination::kSystemMemory:
      return "CPU";
    case viz::CopyOutputRequest::ResultDestination::kNativeTextures:
      return "GPU";
  }
}

}  // namespace

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
  // I420_PLANES via textures is not yet supported.
  DCHECK(result_format_ != ResultFormat::I420_PLANES ||
         result_destination_ == ResultDestination::kSystemMemory);

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

std::string CopyOutputRequest::ToString() const {
  return base::StringPrintf(
      "[%s] -%s→%s-> [%s] @ [%s, %s] %s",
      has_area() ? area().ToString().c_str() : "noclip",
      scale_from().ToString().c_str(), scale_to().ToString().c_str(),
      has_result_selection() ? result_selection().ToString().c_str()
                             : "noclamp",
      ResultFormatToShortString(result_format()),
      ResultDestinationToShortString(result_destination()),
      has_blit_request() ? blit_request_->ToString().c_str() : "noblit");
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

void CopyOutputRequest::set_blit_request(BlitRequest blit_request) {
  DCHECK(!blit_request_);
  DCHECK_EQ(result_destination(), ResultDestination::kNativeTextures);
  DCHECK_EQ(result_format(), ResultFormat::NV12_PLANES);
  DCHECK(has_result_selection());

  // Destination region must start at an even offset for NV12 results:
  DCHECK_EQ(blit_request.destination_region_offset().x() % 2, 0);
  DCHECK_EQ(blit_request.destination_region_offset().y() % 2, 0);

#if DCHECK_IS_ON()
  {
    const gpu::MailboxHolder* first_zeroed_mailbox_it =
        base::ranges::find_if(blit_request.mailboxes(), &gpu::Mailbox::IsZero,
                              &gpu::MailboxHolder::mailbox);

    size_t num_nonzeroed_mailboxes =
        first_zeroed_mailbox_it - blit_request.mailboxes().begin();

    switch (result_format()) {
      case ResultFormat::RGBA:
        DCHECK_EQ(num_nonzeroed_mailboxes, CopyOutputResult::kRGBAMaxPlanes);
        break;
      case ResultFormat::NV12_PLANES:
        DCHECK_EQ(num_nonzeroed_mailboxes, CopyOutputResult::kNV12MaxPlanes);
        break;
      case ResultFormat::I420_PLANES:
        DCHECK_EQ(num_nonzeroed_mailboxes, CopyOutputResult::kI420MaxPlanes);
        break;
    }
  }
#endif

  blit_request_ = std::move(blit_request);
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

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/copy_output_request.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

constexpr int kMaxPendingSendResult = 4;

const char* ResultFormatToShortString(
    viz::CopyOutputRequest::ResultFormat result_format) {
  switch (result_format) {
    case viz::CopyOutputRequest::ResultFormat::RGBA:
      return "RGBA";
    case viz::CopyOutputRequest::ResultFormat::I420_PLANES:
      return "I420";
    case viz::CopyOutputRequest::ResultFormat::NV12:
      return "NV12";
    case viz::CopyOutputRequest::ResultFormat::RGBAF16:
      return "RGBAF16";
  }
}

const char* ResultDestinationToShortString(
    viz::CopyOutputRequest::ResultDestination result_destination) {
  switch (result_destination) {
    case viz::CopyOutputRequest::ResultDestination::kSystemMemory:
      return "CPU";
    case viz::CopyOutputRequest::ResultDestination::kSharedImage:
      return "GPU";
  }
}

int g_pending_send_result_count = 0;

base::Lock& GetPendingSendResultLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

}  // namespace

namespace viz {

CopyOutputRequest::CopyOutputRequest(ResultFormat result_format,
                                     ResultDestination result_destination,
                                     CopyOutputRequestCallback result_callback)
    : result_format_(result_format),
      result_destination_(result_destination),
      result_callback_(std::move(result_callback)),
      result_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      scale_from_(1, 1),
      scale_to_(1, 1) {
  // If format is I420_PLANES, the result must be in system memory. Returning
  // I420_PLANES via textures is not yet supported.
  DCHECK(result_format_ != ResultFormat::I420_PLANES ||
         result_destination_ == ResultDestination::kSystemMemory);

  DCHECK(!result_callback_.is_null());
  TRACE_EVENT_BEGIN("viz", "CopyOutputRequest",
                    perfetto::Track::FromPointer(this));
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
  DCHECK_EQ(result_destination(), ResultDestination::kSharedImage);
  DCHECK(result_format() == ResultFormat::NV12 ||
         result_format() == ResultFormat::RGBA ||
         result_format() == ResultFormat::RGBAF16);
  DCHECK(has_result_selection());

  if (result_format() == ResultFormat::NV12) {
    // Destination region must start at an even offset for NV12 results:
    DCHECK_EQ(blit_request.destination_region_offset().x() % 2, 0);
    DCHECK_EQ(blit_request.destination_region_offset().y() % 2, 0);
  }

  CHECK(blit_request.shared_image());

  blit_request_ = std::move(blit_request);
}

void CopyOutputRequest::SendResult(std::unique_ptr<CopyOutputResult> result) {
  TRACE_EVENT_END("viz",
                  /* CopyOutputRequest */ perfetto::Track::FromPointer(this),
                  "success", !result->IsEmpty(), "has_provided_task_runner",
                  !!result_task_runner_);
  CHECK(result_task_runner_);
  auto task = base::BindOnce(std::move(result_callback_), std::move(result));

  if (send_result_delay_.is_zero()) {
    result_task_runner_->PostTask(FROM_HERE, std::move(task));
  } else {
    base::AutoLock locked_counter(GetPendingSendResultLock());
    if (g_pending_send_result_count >= kMaxPendingSendResult) {
      result_task_runner_->PostTask(FROM_HERE, std::move(task));
    } else {
      g_pending_send_result_count++;
      result_task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              [](base::OnceClosure callback) {
                std::move(callback).Run();
                base::AutoLock locked_counter(GetPendingSendResultLock());
                g_pending_send_result_count--;
              },
              std::move(task)),
          send_result_delay_);
    }
  }
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

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface_presentation_time_recorder.h"

#include <cstdint>
#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/gfx/presentation_feedback.h"

namespace exo {
namespace {

constexpr char kTraceCategory[] = "benchmark,ui";

base::HistogramBase* CreateTimesHistogram(const char* name) {
  return base::Histogram::FactoryTimeGet(
      name, base::Milliseconds(1), base::Milliseconds(200), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

// HistogramReporter reports latency and optional max latency as UMA histograms.
class HistogramReporter
    : public ShellSurfacePresentationTimeRecorder::Reporter {
 public:
  HistogramReporter(const char* latency_histogram_name,
                    std::optional<const char*> max_latency_histogram_name)
      : latency_histogram_(CreateTimesHistogram(latency_histogram_name)),
        max_latency_histogram_name_(max_latency_histogram_name) {}

  HistogramReporter(const HistogramReporter&) = delete;
  HistogramReporter& operator=(const HistogramReporter&) = delete;

  ~HistogramReporter() override {
    if (max_latency_histogram_name_.has_value()) {
      CreateTimesHistogram(max_latency_histogram_name_.value())
          ->AddTimeMillisecondsGranularity(max_latency_);
    }
  }

  // PresentationTimeRecorder::Reporter
  void ReportTime(base::TimeDelta delta) override {
    latency_histogram_->AddTimeMillisecondsGranularity(delta);

    if (max_latency_histogram_name_.has_value() && delta > max_latency_) {
      max_latency_ = delta;
    }
  }

 private:
  const raw_ptr<base::HistogramBase> latency_histogram_;
  const std::optional<const char*> max_latency_histogram_name_;
  base::TimeDelta max_latency_;
};

}  // namespace

// static
std::unique_ptr<ShellSurfacePresentationTimeRecorder::Reporter>
ShellSurfacePresentationTimeRecorder::CreateHistogramReporter(
    const char* latency_histogram_name,
    std::optional<const char*> max_latency_histogram_name) {
  return std::make_unique<HistogramReporter>(latency_histogram_name,
                                             max_latency_histogram_name);
}

ShellSurfacePresentationTimeRecorder::ShellSurfacePresentationTimeRecorder(
    ShellSurface* shell_surface,
    std::unique_ptr<Reporter> reporter)
    : shell_surface_(shell_surface), reporter_(std::move(reporter)) {
  scoped_observation_.Observe(shell_surface_.get());
}

ShellSurfacePresentationTimeRecorder::~ShellSurfacePresentationTimeRecorder() =
    default;

void ShellSurfacePresentationTimeRecorder::PrepareToRecord() {
  if (pending_request_.has_value())
    return;

  pending_request_ = std::make_optional<Request>();
  pending_request_->request_id = next_request_id_++;
}

bool ShellSurfacePresentationTimeRecorder::RequestNext() {
  // Underlying ShellSurface must still be alive.
  DCHECK(shell_surface_);
  // `PrepareToRecord()` must have happened.
  DCHECK(pending_request_.has_value());

  // Early out if there is a pending request that does not get a Configure.
  if (!pending_request_->serial.has_value())
    return false;

  TRACE_EVENT_BEGIN(kTraceCategory, "ShellSurfacePresentationTimeRecorder",
                    perfetto::Track(pending_request_->request_id), "serial",
                    pending_request_->serial.value());

  pending_request_->request_time = base::TimeTicks::Now();
  requests_.emplace_back(pending_request_.value());
  pending_request_.reset();

  LOG_IF(WARNING, requests_.size() > 100u)
      << "Number of requests waiting for ack has reached: " << requests_.size();

  return true;
}

void ShellSurfacePresentationTimeRecorder::OnConfigure(uint32_t serial) {
  if (!pending_request_.has_value())
    return;

  pending_request_->serial = serial;
}

void ShellSurfacePresentationTimeRecorder::OnAcknowledgeConfigure(
    uint32_t serial) {
  // Must not ack a serial in `pending_request_`. RequestNext() should happen
  // to commit the `pending_request_` before the ack.
  DCHECK(!pending_request_.has_value() ||
         !pending_request_->serial.has_value() ||
         serial > pending_request_->serial);

  Surface* root_surface = shell_surface_->root_surface();
  while (!requests_.empty()) {
    Request request = requests_.front();
    requests_.pop_front();

    root_surface->RequestPresentationCallback(base::BindRepeating(
        &ShellSurfacePresentationTimeRecorder::OnFramePresented,
        weak_ptr_factory_.GetWeakPtr(), request));

    if (request.serial.value() == serial)
      break;
  }
}

void ShellSurfacePresentationTimeRecorder::OnShellSurfaceDestroyed() {
  scoped_observation_.Reset();
}

void ShellSurfacePresentationTimeRecorder::OnFramePresented(
    const Request& request,
    const gfx::PresentationFeedback& feedback) {
  TRACE_EVENT_END(kTraceCategory, perfetto::Track(request.request_id), "flags",
                  feedback.flags, "serial", request.serial.value());

  if (feedback.flags & gfx::PresentationFeedback::kFailure) {
    LOG(WARNING) << "PresentationFailed (serial=" << *request.serial << "):"
                 << ", flags=" << feedback.flags;
    return;
  }

  if (feedback.timestamp.is_null()) {
    // TODO(b/165951963): ideally feedback.timestamp should not be null.
    // Consider replacing this by DCHECK or CHECK.
    LOG(ERROR) << "Invalid feedback timestamp (serial=" << *request.serial
               << "):"
               << " timestamp is not set";
    return;
  }

  const base::TimeDelta delta = feedback.timestamp - request.request_time;
  if (delta.InMilliseconds() < 0) {
    LOG(ERROR) << "Invalid timestamp for presentation feedback (serial="
               << *request.serial
               << "): requested_time=" << request.request_time
               << " feedback.timestamp=" << feedback.timestamp;
    return;
  }

  reporter_->ReportTime(delta);
}

}  // namespace exo

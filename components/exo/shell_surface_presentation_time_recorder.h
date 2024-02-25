// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_PRESENTATION_TIME_RECORDER_H_
#define COMPONENTS_EXO_SHELL_SURFACE_PRESENTATION_TIME_RECORDER_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "ash/public/cpp/presentation_time_recorder.h"
#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_observer.h"

namespace gfx {
struct PresentationFeedback;
}

namespace exo {

// ShellSurfacePresentationTimeRecorder records the time of
//   configure-> ack -> present
// of the underlying ShellSurface after a request is made. Requests
// made for the same configure will be ignored.
class ShellSurfacePresentationTimeRecorder
    : public ash::PresentationTimeRecorder,
      public ShellSurfaceObserver {
 public:
  class Reporter {
   public:
    virtual ~Reporter() = default;

    // Invoked to report the time delta.
    virtual void ReportTime(base::TimeDelta delta) = 0;
  };

  // Factory to create histogram reporter.
  static std::unique_ptr<Reporter> CreateHistogramReporter(
      const char* latency_histogram_name,
      std::optional<const char*> max_latency_histogram_name = std::nullopt);

  ShellSurfacePresentationTimeRecorder(ShellSurface* shell_surface,
                                       std::unique_ptr<Reporter> reporter);
  ShellSurfacePresentationTimeRecorder(
      const ShellSurfacePresentationTimeRecorder&) = delete;
  ShellSurfacePresentationTimeRecorder& operator=(
      const ShellSurfacePresentationTimeRecorder&) = delete;

  ~ShellSurfacePresentationTimeRecorder() override;

  // ash::PresentationTimeRecorder:
  void PrepareToRecord() override;
  bool RequestNext() override;

  // ShellSurfaceObserver:
  void OnConfigure(uint32_t serial) override;
  void OnAcknowledgeConfigure(uint32_t serial) override;
  void OnShellSurfaceDestroyed() override;

 protected:
  struct Request {
    uint64_t request_id = 0u;
    // Time when RequestNext is called.
    base::TimeTicks request_time;
    // Serial of the first Configure after RequestNext.
    std::optional<uint32_t> serial = std::nullopt;
  };

  // Invoked to notify a frame is presented to calculate time delta between
  // `request_time` and the frame's presentation feedback.
  // Virtual for testing.
  virtual void OnFramePresented(const Request& request,
                                const gfx::PresentationFeedback& feedback);

 private:
  raw_ptr<ShellSurface, DanglingUntriaged> shell_surface_ = nullptr;
  std::unique_ptr<Reporter> reporter_;

  uint64_t next_request_id_ = 0u;

  // Request waiting for configure. There would be only one such request.
  std::optional<Request> pending_request_;

  // Requests that have received "configure" and wait for "ack".
  base::circular_deque<Request> requests_;

  base::ScopedObservation<ShellSurface, ShellSurfaceObserver>
      scoped_observation_{this};
  base::WeakPtrFactory<ShellSurfacePresentationTimeRecorder> weak_ptr_factory_{
      this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_PRESENTATION_TIME_RECORDER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_PERF_DRAG_EVENT_GENERATOR_H_
#define CHROME_TEST_BASE_PERF_DRAG_EVENT_GENERATOR_H_

#include "base/run_loop.h"
#include "base/time/time.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"

namespace ui_test_utils {

// A utility class that generates drag events using |producer| logic
// at a rate of 60 events per seconds.
class DragEventGenerator {
 public:
  // Producer produces the point for given progress value. The range of
  // the progress is between 0.f (start) to 1.f (end).
  class PointProducer {
   public:
    virtual ~PointProducer();

    // Returns the position at |progression|.
    virtual gfx::Point GetPosition(float progress) = 0;

    // Returns the duration this produce should be used.
    virtual const base::TimeDelta GetDuration() const = 0;
  };

  ~DragEventGenerator();

  // If |hover| is true, do not send a initial mouse press event. Generates
  // events at 120 hertz if |use_120fps|.
  static std::unique_ptr<DragEventGenerator> CreateForMouse(
      std::unique_ptr<PointProducer> producer,
      bool hover = false,
      bool use_120fps = false);
  // If |long_press| is true wait a bit until a long press event is sent before
  // starting dragging.
  static std::unique_ptr<DragEventGenerator> CreateForTouch(
      std::unique_ptr<PointProducer> producer,
      bool long_press = false,
      bool use_120fps = false);

  void Wait();

 private:
  DragEventGenerator(std::unique_ptr<PointProducer> producer,
                     bool touch = false,
                     bool hover = false,
                     bool use_120fps = false,
                     bool long_press = false);

  void Done(const gfx::Point position);
  void GenerateNext();
  base::TimeDelta GetNextFrameDuration() const;

  std::unique_ptr<PointProducer> producer_;
  int count_ = 0;

  // Whether the frame duration corresponds to 120fps (as opposed to 60fps).
  const bool use_120fps_;

  const base::TimeTicks start_;
  base::TimeTicks expected_next_time_;
  const bool touch_;
  const bool hover_;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(DragEventGenerator);
};

// InterpolatedProducer produces the interpolated location between two points
// based on tween type.
class InterpolatedProducer : public DragEventGenerator::PointProducer {
 public:
  InterpolatedProducer(const gfx::Point& start,
                       const gfx::Point& end,
                       const base::TimeDelta duration,
                       gfx::Tween::Type type = gfx::Tween::LINEAR);
  ~InterpolatedProducer() override;

  // PointProducer:
  gfx::Point GetPosition(float progress) override;
  const base::TimeDelta GetDuration() const override;

 private:
  gfx::Point start_, end_;
  base::TimeDelta duration_;
  gfx::Tween::Type type_;

  DISALLOW_COPY_AND_ASSIGN(InterpolatedProducer);
};

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_PERF_DRAG_EVENT_GENERATOR_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/perf/drag_event_generator.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace ui_test_utils {

////////////////////////////////////////////////////////////////////////////////
// DragEventGenerator

DragEventGenerator::~DragEventGenerator() {
  VLOG(1) << "Effective Event per seconds="
          << (count_ * 1000) / producer_->GetDuration().InMilliseconds();
}

// static
std::unique_ptr<DragEventGenerator> DragEventGenerator::CreateForMouse(
    std::unique_ptr<PointProducer> producer,
    bool hover,
    bool use_120fps) {
  return base::WrapUnique(new DragEventGenerator(
      std::move(producer), /*touch=*/false, hover, use_120fps));
}

// static
std::unique_ptr<DragEventGenerator> DragEventGenerator::CreateForTouch(
    std::unique_ptr<PointProducer> producer,
    bool long_press,
    bool use_120fps) {
  return base::WrapUnique(new DragEventGenerator(
      std::move(producer),
      /*touch=*/true, /*hover=*/false, use_120fps, long_press));
}

void DragEventGenerator::Wait() {
  run_loop_.Run();
}

DragEventGenerator::DragEventGenerator(std::unique_ptr<PointProducer> producer,
                                       bool touch,
                                       bool hover,
                                       bool use_120fps,
                                       bool long_press)
    : producer_(std::move(producer)),
      use_120fps_(use_120fps),
      start_(base::TimeTicks::Now()),
      expected_next_time_(start_ + GetNextFrameDuration()),
      touch_(touch),
      hover_(hover) {
  gfx::Point initial_position = producer_->GetPosition(0.f);
  if (touch_) {
    ui_controls::SendTouchEvents(ui_controls::PRESS, 0, initial_position.x(),
                                 initial_position.y());
  } else {
    base::RunLoop run_loop;
    ui_controls::SendMouseMoveNotifyWhenDone(
        initial_position.x(), initial_position.y(), run_loop.QuitClosure());
    run_loop.Run();
    if (!hover_)
      ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::DOWN);
  }

  base::TimeDelta delay = GetNextFrameDuration();
  if (long_press) {
    DCHECK(touch_);
    ui::GestureConfiguration::GetInstance()->set_show_press_delay_in_ms(1);
    ui::GestureConfiguration::GetInstance()->set_long_press_time_in_ms(2);
    delay = base::TimeDelta::FromMilliseconds(5);
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DragEventGenerator::GenerateNext, base::Unretained(this)),
      delay);
}

void DragEventGenerator::Done(const gfx::Point position) {
  if (touch_) {
    ui_controls::SendTouchEventsNotifyWhenDone(ui_controls::RELEASE, 0,
                                               position.x(), position.y(),
                                               run_loop_.QuitClosure());
  } else {
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT, ui_controls::UP, run_loop_.QuitClosure());
  }
}

void DragEventGenerator::GenerateNext() {
  auto now = base::TimeTicks::Now();
  auto elapsed = now - start_;

  expected_next_time_ += GetNextFrameDuration();
  count_++;
  const base::TimeDelta duration = producer_->GetDuration();
  if (elapsed >= duration) {
    gfx::Point position = producer_->GetPosition(1.0f);
    if (touch_) {
      ui_controls::SendTouchEventsNotifyWhenDone(
          ui_controls::MOVE, 0, position.x(), position.y(),
          base::BindOnce(&DragEventGenerator::Done, base::Unretained(this),
                         position));
    } else {
      ui_controls::SendMouseMoveNotifyWhenDone(
          position.x(), position.y(),
          base::BindOnce(&DragEventGenerator::Done, base::Unretained(this),
                         position));
    }
    return;
  }

  float progress =
      static_cast<float>(elapsed.InMilliseconds()) / duration.InMilliseconds();
  gfx::Point position = producer_->GetPosition(progress);
  if (touch_) {
    ui_controls::SendTouchEvents(ui_controls::MOVE, 0, position.x(),
                                 position.y());
  } else {
    ui_controls::SendMouseMove(position.x(), position.y());
  }

  auto delta = expected_next_time_ - now;
  if (delta.InMilliseconds() > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DragEventGenerator::GenerateNext,
                       base::Unretained(this)),
        delta);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DragEventGenerator::GenerateNext,
                                  base::Unretained(this)));
  }
}

base::TimeDelta DragEventGenerator::GetNextFrameDuration() const {
  return base::TimeDelta::FromMicroseconds(use_120fps_ ? 8333 : 16666);
}

////////////////////////////////////////////////////////////////////////////////
// DragEventGenerator::PointProducer

DragEventGenerator::PointProducer::~PointProducer() = default;

////////////////////////////////////////////////////////////////////////////////
// InterpolatedProducer:

InterpolatedProducer::InterpolatedProducer(const gfx::Point& start,
                                           const gfx::Point& end,
                                           const base::TimeDelta duration,
                                           gfx::Tween::Type type)
    : start_(start), end_(end), duration_(duration), type_(type) {}

InterpolatedProducer::~InterpolatedProducer() = default;

gfx::Point InterpolatedProducer::GetPosition(float progress) {
  float value = gfx::Tween::CalculateValue(type_, progress);
  return gfx::Point(
      gfx::Tween::LinearIntValueBetween(value, start_.x(), end_.x()),
      gfx::Tween::LinearIntValueBetween(value, start_.y(), end_.y()));
}

const base::TimeDelta InterpolatedProducer::GetDuration() const {
  return duration_;
}

}  // namespace ui_test_utils

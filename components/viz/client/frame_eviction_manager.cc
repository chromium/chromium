// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/frame_eviction_manager.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"

namespace viz {
namespace {

const int kModeratePressurePercentage = 50;
const int kCriticalPressurePercentage = 10;

}  // namespace

FrameEvictionManager::ScopedPause::ScopedPause() {
  FrameEvictionManager::GetInstance()->Pause();
}

FrameEvictionManager::ScopedPause::~ScopedPause() {
  FrameEvictionManager::GetInstance()->Unpause();
}

FrameEvictionManager* FrameEvictionManager::GetInstance() {
  return base::Singleton<FrameEvictionManager>::get();
}

FrameEvictionManager::~FrameEvictionManager() = default;

void FrameEvictionManager::AddFrame(FrameEvictionManagerClient* frame,
                                    bool locked) {
  RemoveFrame(frame);
  if (locked)
    locked_frames_[frame] = 1;
  else
    RegisterUnlockedFrame(frame);
  CullUnlockedFrames(GetMaxNumberOfSavedFrames());
}

void FrameEvictionManager::RemoveFrame(FrameEvictionManagerClient* frame) {
  auto locked_iter = locked_frames_.find(frame);
  if (locked_iter != locked_frames_.end())
    locked_frames_.erase(locked_iter);
  unlocked_frames_.remove_if([&](const auto& p) { return p.first == frame; });
}

void FrameEvictionManager::LockFrame(FrameEvictionManagerClient* frame) {
  if (base::Contains(unlocked_frames_, frame,
                     [](const auto& p) { return p.first; })) {
    DCHECK(locked_frames_.find(frame) == locked_frames_.end());
    unlocked_frames_.remove_if([&](const auto& p) { return p.first == frame; });
    locked_frames_[frame] = 1;
  } else {
    DCHECK(locked_frames_.find(frame) != locked_frames_.end());
    locked_frames_[frame]++;
  }
}

void FrameEvictionManager::UnlockFrame(FrameEvictionManagerClient* frame) {
  DCHECK(locked_frames_.find(frame) != locked_frames_.end());
  size_t locked_count = locked_frames_[frame];
  DCHECK(locked_count);
  if (locked_count > 1) {
    locked_frames_[frame]--;
  } else {
    RemoveFrame(frame);
    RegisterUnlockedFrame(frame);
    CullUnlockedFrames(GetMaxNumberOfSavedFrames());
  }
}

void FrameEvictionManager::RegisterUnlockedFrame(
    FrameEvictionManagerClient* frame) {
  unlocked_frames_.emplace_front(frame, clock_->NowTicks());
  if (base::FeatureList::IsEnabled(features::kAggressiveFrameCulling)) {
    if (!idle_frames_culling_timer_.IsRunning()) {
      // Unretained: `idle_frames_culling_timer_` is a member of `this`, doesn't
      // outlive it, and cancels the task in its destructor.
      idle_frames_culling_timer_.Start(
          FROM_HERE, kPeriodicCullingDelay,
          base::BindRepeating(&FrameEvictionManager::CullOldUnlockedFrames,
                              base::Unretained(this)));
    }
  }
}

size_t FrameEvictionManager::GetMaxNumberOfSavedFrames() const {
  int percentage = 100;
  base::MemoryPressureMonitor* monitor = base::MemoryPressureMonitor::Get();

  if (!monitor)
    return max_number_of_saved_frames_;

  // Until we have a global OnMemoryPressureChanged event we need to query the
  // value from our specific pressure monitor.
  switch (monitor->GetCurrentPressureLevel()) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      percentage = 100;
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      percentage = kModeratePressurePercentage;
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      percentage = kCriticalPressurePercentage;
      break;
  }
  size_t frames = (max_number_of_saved_frames_ * percentage) / 100;
  return std::max(static_cast<size_t>(1), frames);
}

FrameEvictionManager::FrameEvictionManager()
    : memory_pressure_listener_(new base::MemoryPressureListener(
          FROM_HERE,
          base::BindRepeating(&FrameEvictionManager::OnMemoryPressure,
                              base::Unretained(this)))) {
  max_number_of_saved_frames_ =
#if BUILDFLAG(IS_ANDROID)
      // If the amount of memory on the device is >= 3.5 GB, save up to 5
      // frames.
      base::SysInfo::AmountOfPhysicalMemoryMB() < 1024 * 3.5f ? 1 : 5;
#else
      std::min(5, 2 + (base::SysInfo::AmountOfPhysicalMemoryMB() / 256));
#endif
}

void FrameEvictionManager::CullUnlockedFrames(size_t saved_frame_limit) {
  if (pause_count_) {
    pending_unlocked_frame_limit_ = saved_frame_limit;
    return;
  }

  while (!unlocked_frames_.empty() &&
         unlocked_frames_.size() + locked_frames_.size() > saved_frame_limit) {
    size_t old_size = unlocked_frames_.size();
    // Should remove self from list.
    auto* frame = unlocked_frames_.back().first;
    frame->EvictCurrentFrame();
    if (unlocked_frames_.size() == old_size)
      break;
  }
}

void FrameEvictionManager::CullOldUnlockedFrames() {
  DCHECK(std::is_sorted(
      unlocked_frames_.begin(), unlocked_frames_.end(),
      [](const auto& a, const auto& b) { return a.second >= b.second; }));

  // Try again later, since the timer is not cancelled.
  if (pause_count_)
    return;

  auto now = clock_->NowTicks();
  while (!unlocked_frames_.empty() &&
         now - unlocked_frames_.back().second >= kPeriodicCullingDelay) {
    size_t old_size = unlocked_frames_.size();
    auto* frame = unlocked_frames_.back().first;
    frame->EvictCurrentFrame();
    // Should remove self from list. If it's not possible, give up and try again
    // later. This should be a rare case, so don't bother rescheduling earlier
    // than the next timer tick.
    //
    // See https://chromium-review.googlesource.com/c/chromium/src/+/2585790 for
    // an example where this can happen.
    if (old_size - 1 != unlocked_frames_.size())
      break;
  }

  if (unlocked_frames_.empty())
    idle_frames_culling_timer_.Stop();
}

void FrameEvictionManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      PurgeMemory(kModeratePressurePercentage);
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      if (base::FeatureList::IsEnabled(features::kAggressiveFrameCulling))
        PurgeAllUnlockedFrames();
      else
        PurgeMemory(kCriticalPressurePercentage);
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      // No need to change anything when there is no pressure.
      return;
  }
}

void FrameEvictionManager::PurgeMemory(int percentage) {
  int saved_frame_limit = max_number_of_saved_frames_;
  int remaining_frames = std::max(1, (saved_frame_limit * percentage) / 100);

  if (saved_frame_limit <= 1)
    return;

  CullUnlockedFrames(remaining_frames);
}

void FrameEvictionManager::PurgeAllUnlockedFrames() {
  CullUnlockedFrames(0);
}

void FrameEvictionManager::SetOverridesForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* clock) {
  idle_frames_culling_timer_.SetTaskRunner(task_runner);
  clock_ = clock;
}

void FrameEvictionManager::Pause() {
  ++pause_count_;
}

void FrameEvictionManager::Unpause() {
  --pause_count_;
  DCHECK_GE(pause_count_, 0);

  if (pause_count_ == 0 && pending_unlocked_frame_limit_) {
    CullUnlockedFrames(pending_unlocked_frame_limit_.value());
    pending_unlocked_frame_limit_.reset();
  }
}

}  // namespace viz

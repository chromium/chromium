// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_FRAME_EVICTION_MANAGER_H_
#define COMPONENTS_VIZ_CLIENT_FRAME_EVICTION_MANAGER_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/post_delayed_memory_reduction_task.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "components/viz/client/viz_client_export.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/pre_freeze_background_memory_trimmer.h"
#endif

namespace viz {

class FrameEvictionManagerClient {
 public:
  virtual ~FrameEvictionManagerClient() {}
  virtual void EvictCurrentFrame() = 0;
};

// This class is responsible for globally managing which renderers keep their
// compositor frame when offscreen. We actively discard compositor frames for
// offscreen tabs, but keep a minimum amount, as an LRU cache, to make switching
// between a small set of tabs faster. The limit is a soft limit, because
// clients can lock their frame to prevent it from being discarded, e.g. if the
// tab is visible, or while capturing a screenshot.
class VIZ_CLIENT_EXPORT FrameEvictionManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  // Pauses frame eviction within its scope.
  class VIZ_CLIENT_EXPORT ScopedPause {
   public:
    ScopedPause();

    ScopedPause(const ScopedPause&) = delete;
    ScopedPause& operator=(const ScopedPause&) = delete;

    ~ScopedPause();
  };

  static FrameEvictionManager* GetInstance();

  FrameEvictionManager(const FrameEvictionManager&) = delete;
  FrameEvictionManager& operator=(const FrameEvictionManager&) = delete;

  void AddFrame(FrameEvictionManagerClient*, bool locked);
  void RemoveFrame(FrameEvictionManagerClient*);
  void LockFrame(FrameEvictionManagerClient*);
  void UnlockFrame(FrameEvictionManagerClient*);

  size_t GetMaxNumberOfSavedFrames() const;

  // For testing only
  void set_max_number_of_saved_frames(size_t max_number_of_saved_frames) {
    max_number_of_saved_frames_ = max_number_of_saved_frames;
  }

  // React on memory pressure events to adjust the number of cached frames.
  // Please make this private when crbug.com/443824 has been fixed.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Purges all unlocked frames, allowing us to reclaim resources.
  void PurgeAllUnlockedFrames();

  // Chosen arbitrarily, didn't show regressions in metrics during a field trial
  // in 2023. Should ideally be higher than a common time to switch between
  // tabs. The reasoning is that if a tab isn't switched to in this delay, then
  // it's unlikeky to soon be.
  static constexpr base::TimeDelta kPeriodicCullingDelay = base::Minutes(5);

 private:
  friend struct base::DefaultSingletonTraits<FrameEvictionManager>;
  FRIEND_TEST_ALL_PREFIXES(FrameEvictionManagerTest, PeriodicCulling);

  FrameEvictionManager();
  ~FrameEvictionManager() override;

  void StartFrameCullingTimer();
  void CullUnlockedFrames(size_t saved_frame_limit);
#if BUILDFLAG(IS_ANDROID)
  void CullOldUnlockedFrames(base::MemoryReductionTaskContext task_type);
#else
  void CullOldUnlockedFrames();
#endif

  void PurgeMemory(int percentage);

  // Pauses/unpauses frame eviction.
  void Pause();
  void Unpause();

  void RegisterUnlockedFrame(FrameEvictionManagerClient* frame);

  // Inject mock versions for testing.
  void SetOverridesForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::TickClock* clock);

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Listens for system under pressure notifications and adjusts number of
  // cached frames accordingly.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  std::map<FrameEvictionManagerClient*, size_t> locked_frames_;
  // {FrameEvictionManagerClient, Last Unlock() time}, ordered with the most
  // recent first.
  std::list<std::pair<FrameEvictionManagerClient*, base::TimeTicks>>
      unlocked_frames_;
  size_t max_number_of_saved_frames_;

  // Counter of the outstanding pauses.
  int pause_count_ = 0;

  // Argument of the last CullUnlockedFrames call while paused.
  std::optional<size_t> pending_unlocked_frame_limit_;

  base::OneShotDelayedBackgroundTimer idle_frame_culling_timer_;
  raw_ptr<const base::TickClock> clock_ = base::DefaultTickClock::GetInstance();
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_FRAME_EVICTION_MANAGER_H_

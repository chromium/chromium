// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_FRAME_EVICTION_MANAGER_H_
#define COMPONENTS_VIZ_CLIENT_FRAME_EVICTION_MANAGER_H_

#include <stddef.h>

#include <list>
#include <map>

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "components/viz/client/viz_client_export.h"

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
class VIZ_CLIENT_EXPORT FrameEvictionManager {
 public:
  // Pauses frame eviction within its scope.
  class VIZ_CLIENT_EXPORT ScopedPause {
   public:
    ScopedPause();
    ~ScopedPause();

   private:
    DISALLOW_COPY_AND_ASSIGN(ScopedPause);
  };

  static FrameEvictionManager* GetInstance();

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

 private:
  friend struct base::DefaultSingletonTraits<FrameEvictionManager>;

  FrameEvictionManager();
  ~FrameEvictionManager();

  void CullUnlockedFrames(size_t saved_frame_limit);

  void PurgeMemory(int percentage);

  // Pauses/unpauses frame eviction.
  void Pause();
  void Unpause();

  // Listens for system under pressure notifications and adjusts number of
  // cached frames accordingly.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  std::map<FrameEvictionManagerClient*, size_t> locked_frames_;
  std::list<FrameEvictionManagerClient*> unlocked_frames_;
  size_t max_number_of_saved_frames_;

  // Counter of the outstanding pauses.
  int pause_count_ = 0;

  // Argument of the last CullUnlockedFrames call while paused.
  base::Optional<size_t> pending_unlocked_frame_limit_;

  DISALLOW_COPY_AND_ASSIGN(FrameEvictionManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_FRAME_EVICTION_MANAGER_H_

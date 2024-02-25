// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_VSYNC_TIMING_MANAGER_H_
#define COMPONENTS_EXO_VSYNC_TIMING_MANAGER_H_

#include <vector>

#include "ash/frame_throttler/frame_throttling_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/vsync_parameter_observer.mojom.h"

namespace exo {

// Multiplexes vsync parameter updates from the display compositor to exo
// clients using the zcr_vsync_feedback_v1 protocol. Will maintain an IPC
// connection to the display compositor only when necessary.
class VSyncTimingManager : public viz::mojom::VSyncParameterObserver,
                           public ash::FrameThrottlingObserver {
 public:
  // Will be notified about changes in vsync parameters.
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnUpdateVSyncParameters(base::TimeTicks timebase,
                                         base::TimeDelta interval) = 0;
  };

  // Used to setup IPC connection to display compositor.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void AddVSyncParameterObserver(
        mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer) = 0;
  };

  explicit VSyncTimingManager(Delegate* delegate);

  VSyncTimingManager(const VSyncTimingManager&) = delete;
  VSyncTimingManager& operator=(const VSyncTimingManager&) = delete;

  ~VSyncTimingManager() override;

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  base::TimeDelta throttled_interval() const { return throttled_interval_; }

 private:
  // Overridden from viz::mojom::VSyncParameterObserver:
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override;

  // Overridden from ash::FrameThrottlingObserver
  void OnThrottlingStarted(const std::vector<aura::Window*>& windows,
                           uint8_t fps) override;
  void OnThrottlingEnded() override;

  void InitializeConnection();
  void MaybeInitializeConnection();
  void OnConnectionError();

  base::TimeDelta throttled_interval_;
  base::TimeDelta last_interval_;
  base::TimeTicks last_timebase_;

  const raw_ptr<Delegate> delegate_;

  std::vector<raw_ptr<Observer, VectorExperimental>> observers_;

  mojo::Receiver<viz::mojom::VSyncParameterObserver> receiver_{this};

  base::WeakPtrFactory<VSyncTimingManager> weak_ptr_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_VSYNC_TIMING_MANAGER_H_

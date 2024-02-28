// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/vsync_timing_manager.h"

#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace exo {

VSyncTimingManager::VSyncTimingManager(Delegate* delegate)
    : last_interval_(viz::BeginFrameArgs::DefaultInterval()),
      delegate_(delegate) {}

VSyncTimingManager::~VSyncTimingManager() = default;

void VSyncTimingManager::AddObserver(Observer* obs) {
  DCHECK(obs);

  // This is adding the first observer so start receiving IPCs.
  if (observers_.empty())
    InitializeConnection();

  observers_.push_back(obs);
}

void VSyncTimingManager::RemoveObserver(Observer* obs) {
  DCHECK(obs);

  std::erase(observers_, obs);

  // There are no more observers so stop receiving IPCs.
  if (observers_.empty())
    receiver_.reset();
}

void VSyncTimingManager::OnUpdateVSyncParameters(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  for (exo::VSyncTimingManager::Observer* observer : observers_) {
    observer->OnUpdateVSyncParameters(timebase, throttled_interval_.is_zero()
                                                    ? interval
                                                    : throttled_interval_);
  }
  last_timebase_ = timebase;
  last_interval_ = interval;
}

void VSyncTimingManager::OnThrottlingStarted(
    const std::vector<aura::Window*>& windows,
    uint8_t fps) {
  throttled_interval_ = base::Hertz(fps);
  OnUpdateVSyncParameters(last_timebase_, last_interval_);
}

void VSyncTimingManager::OnThrottlingEnded() {
  throttled_interval_ = base::TimeDelta();
  OnUpdateVSyncParameters(last_timebase_, last_interval_);
}

void VSyncTimingManager::InitializeConnection() {
  mojo::PendingRemote<viz::mojom::VSyncParameterObserver> remote =
      receiver_.BindNewPipeAndPassRemote();

  // Unretained is safe because |this| owns |receiver_| and will outlive it.
  receiver_.set_disconnect_handler(base::BindOnce(
      &VSyncTimingManager::OnConnectionError, base::Unretained(this)));

  delegate_->AddVSyncParameterObserver(std::move(remote));
}

void VSyncTimingManager::MaybeInitializeConnection() {
  // The last observer might have been unregistered between when there was a
  // connection error, in which case we don't need to reconnect. Alternatively,
  // the last observer might have been unregistered and then a new observer
  // registered, in which case we already reconnected.
  if (!observers_.empty() || receiver_.is_bound())
    InitializeConnection();
}

void VSyncTimingManager::OnConnectionError() {
  receiver_.reset();

  // Try to add a new observer after a short delay. If adding a new observer
  // fails we'll retry again until successful. The delay avoids spamming
  // retries.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VSyncTimingManager::MaybeInitializeConnection,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(250));
}

}  // namespace exo

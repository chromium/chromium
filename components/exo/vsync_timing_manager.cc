// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/vsync_timing_manager.h"

#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"

namespace exo {

VSyncTimingManager::VSyncTimingManager(Delegate* delegate)
    : delegate_(delegate) {}

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

  base::Erase(observers_, obs);

  // There are no more observers so stop receiving IPCs.
  if (observers_.empty())
    receiver_.reset();
}

void VSyncTimingManager::OnUpdateVSyncParameters(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  for (auto* observer : observers_)
    observer->OnUpdateVSyncParameters(timebase, interval);
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
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VSyncTimingManager::MaybeInitializeConnection,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(250));
}

}  // namespace exo

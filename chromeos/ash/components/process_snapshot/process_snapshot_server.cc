// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/process_snapshot/process_snapshot_server.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

base::LazyInstance<ProcessSnapshotServer>::DestructorAtExit g_instance =
    LAZY_INSTANCE_INITIALIZER;

// TODO(afakhry): Figure out if we can build a snapshot only the first time,
// then use CONFIG_PROC_EVENTS to listen to subsequent fork()s or exec()s in
// order to avoid having to regularly walk /proc fs to build the snapshot, and
// to avoid having a stale |snaphot_| in between two refreshes.
base::ProcessIterator::ProcessEntries GetProcessSnapshot() {
  return base::ProcessIterator(/*process_filter=*/nullptr).Snapshot();
}

}  // namespace

// -----------------------------------------------------------------------------
// ProcessSnapshotServer::Observer:

ProcessSnapshotServer::Observer::Observer(base::TimeDelta desired_refresh_time)
    : desired_refresh_time_(desired_refresh_time) {}

// -----------------------------------------------------------------------------
// ProcessSnapshotServer:

ProcessSnapshotServer::~ProcessSnapshotServer() = default;

// static
ProcessSnapshotServer* ProcessSnapshotServer::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_instance.Pointer();
}

void ProcessSnapshotServer::AddObserver(Observer* observer) {
  base::TimeDelta current_refresh_time =
      timer_.IsRunning() ? timer_.GetCurrentDelay() : base::TimeDelta::Max();

  observers_.AddObserver(observer);

  // Only refresh the timer if this observer requires a higher refresh rate.
  if (observer->desired_refresh_time() > current_refresh_time)
    return;

  RefreshTimer(observer->desired_refresh_time());
}

void ProcessSnapshotServer::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);

  base::TimeDelta min_refresh_time = base::TimeDelta::Max();
  for (const auto& remaining_observer : observers_) {
    min_refresh_time =
        std::min(min_refresh_time, remaining_observer.desired_refresh_time());
  }

  RefreshTimer(min_refresh_time);
}

ProcessSnapshotServer::ProcessSnapshotServer()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      weak_ptr_factory_(this) {}

void ProcessSnapshotServer::RefreshSnapshot() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetProcessSnapshot),
      base::BindOnce(&ProcessSnapshotServer::OnSnapshotRefreshed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProcessSnapshotServer::OnSnapshotRefreshed(
    base::ProcessIterator::ProcessEntries snapshot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  snapshot_ = std::move(snapshot);

  for (auto& observer : observers_)
    observer.OnProcessSnapshotRefreshed(snapshot_);
}

void ProcessSnapshotServer::RefreshTimer(base::TimeDelta new_refresh_time) {
  if (new_refresh_time == base::TimeDelta::Max()) {
    timer_.Stop();
    return;
  }

  // First time observer (i.e. when timer is not running) should trigger an
  // immediate refresh.
  if (!timer_.IsRunning())
    RefreshSnapshot();
  else if (new_refresh_time == timer_.GetCurrentDelay())
    return;

  timer_.Start(FROM_HERE, new_refresh_time,
               base::BindRepeating(&ProcessSnapshotServer::RefreshSnapshot,
                                   base::Unretained(this)));
}

}  // namespace ash

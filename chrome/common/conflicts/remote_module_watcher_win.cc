// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/conflicts/remote_module_watcher_win.h"

#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"

namespace {

// Note: Can be called on any threads, even those not owned by the task
//       scheduler.
void OnModuleEvent(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const ModuleWatcher::OnModuleEventCallback& on_module_event_callback,
    const ModuleWatcher::ModuleEvent& event) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(on_module_event_callback, event));
}

}  // namespace

// static
constexpr base::TimeDelta RemoteModuleWatcher::kIdleDelay;

RemoteModuleWatcher::~RemoteModuleWatcher() = default;

// static
RemoteModuleWatcher::UniquePtr RemoteModuleWatcher::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingRemote<mojom::ModuleEventSink> remote_sink) {
  auto remote_module_watcher =
      UniquePtr(new RemoteModuleWatcher(task_runner),
                base::OnTaskRunnerDeleter(task_runner));

  // Because |remote_module_watcher| will be sent for deletion on |task_runner|,
  // using an unretained pointer to it is safe as the initialization is
  // guaranteed to be run before the destructor.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&RemoteModuleWatcher::InitializeOnTaskRunner,
                                base::Unretained(remote_module_watcher.get()),
                                std::move(remote_sink)));

  return remote_module_watcher;
}

RemoteModuleWatcher::RemoteModuleWatcher(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      delay_timer_(FROM_HERE,
                   kIdleDelay,
                   this,
                   &RemoteModuleWatcher::OnTimerFired) {}

void RemoteModuleWatcher::InitializeOnTaskRunner(
    mojo::PendingRemote<mojom::ModuleEventSink> remote_sink) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  module_event_sink_.Bind(std::move(remote_sink));
  module_watcher_ = ModuleWatcher::Create(base::BindRepeating(
      &OnModuleEvent, task_runner_,
      base::BindRepeating(&RemoteModuleWatcher::HandleModuleEvent,
                          weak_ptr_factory_.GetWeakPtr())));
}

void RemoteModuleWatcher::HandleModuleEvent(
    const ModuleWatcher::ModuleEvent& event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Accumulate events. They will be sent when the |delay_timer_| fires.
  module_load_addresses_.push_back(
      reinterpret_cast<uintptr_t>(event.module_load_address));

  // Ensure the timer is running.
  delay_timer_.Reset();
}

void RemoteModuleWatcher::OnTimerFired() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  module_event_sink_->OnModuleEvents(module_load_addresses_);
  module_load_addresses_.clear();
}

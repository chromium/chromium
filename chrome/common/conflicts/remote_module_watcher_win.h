// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONFLICTS_REMOTE_MODULE_WATCHER_WIN_H_
#define CHROME_COMMON_CONFLICTS_REMOTE_MODULE_WATCHER_WIN_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/common/conflicts/module_event_sink_win.mojom.h"
#include "chrome/common/conflicts/module_watcher_win.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SingleThreadTaskRunner;
struct OnTaskRunnerDeleter;
}  // namespace base

// This class is used to instantiate a ModuleWatcher instance in a child
// process that forwards all the module events to the browser process via the
// mojom::ModuleEventSink interface.
class RemoteModuleWatcher {
 public:
  // Provided for convenience.
  using UniquePtr =
      std::unique_ptr<RemoteModuleWatcher, base::OnTaskRunnerDeleter>;

  // The amount of time this class waits before sending all the received module
  // events in one batch to the browser process.
  static constexpr base::TimeDelta kIdleDelay = base::TimeDelta::FromSeconds(5);

  ~RemoteModuleWatcher();

  // Creates a RemoteModuleWatcher instance and initializes it on |task_runner|.
  // The instance lives on that task runner and will be destroyed there when the
  // UniquePtr is destroyed.
  static UniquePtr Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojo::PendingRemote<mojom::ModuleEventSink> remote_sink);

 private:
  explicit RemoteModuleWatcher(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Initializes this instance by connecting the |module_event_sink_| instance
  // and starting the |module_watcher_|. Called on |task_runner_|.
  void InitializeOnTaskRunner(
      mojo::PendingRemote<mojom::ModuleEventSink> remote_sink);

  // Receives module load events from the |module_watcher_| and forwards them to
  // the |module_event_sink_|.
  void HandleModuleEvent(const ModuleWatcher::ModuleEvent& event);

  // Sends all accumulated module events in |module_load_addresses_| to the
  // |module_event_sink_| in one batch.
  void OnTimerFired();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Module events from |module_watcher_| are forwarded to the browser process
  // through this sink.
  mojo::Remote<mojom::ModuleEventSink> module_event_sink_;

  // Observes module load events.
  std::unique_ptr<ModuleWatcher> module_watcher_;

  // Accumulates module events. They will be sent to the browser process when
  // |delay_timer_| fires.
  std::vector<uint64_t> module_load_addresses_;

  // This timer is used to delay the sending of module events until none have
  // been received for |kIdleDelay| amount of time.
  base::DelayTimer delay_timer_;

  base::WeakPtrFactory<RemoteModuleWatcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RemoteModuleWatcher);
};

#endif  // CHROME_COMMON_CONFLICTS_REMOTE_MODULE_WATCHER_WIN_H_

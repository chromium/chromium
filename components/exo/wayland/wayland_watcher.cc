// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_watcher.h"

#include "base/task/current_thread.h"
#include "components/exo/wayland/server.h"

namespace exo {
namespace wayland {

WaylandWatcher::WaylandWatcher(wayland::Server* server)
    : controller_(FROM_HERE), server_(server) {
  Start();
}

WaylandWatcher::~WaylandWatcher() {
  controller_.StopWatchingFileDescriptor();
}

void WaylandWatcher::StartForTesting() {
  Start();
}

void WaylandWatcher::StopForTesting() {
  controller_.StopWatchingFileDescriptor();
}

void WaylandWatcher::Start() {
  base::CurrentUIThread::Get()->WatchFileDescriptor(
      server_->GetFileDescriptor(),
      true,  // persistent
      base::MessagePumpForUI::WATCH_READ, &controller_, this);
}

void WaylandWatcher::OnFileCanReadWithoutBlocking(int fd) {
  server_->Dispatch(base::TimeDelta());
  server_->Flush();
}

void WaylandWatcher::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace wayland
}  // namespace exo

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_WATCHER_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_WATCHER_H_

#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"

namespace exo {
namespace wayland {

class Server;

class WaylandWatcher : public base::MessagePumpForUI::FdWatcher {
 public:
  explicit WaylandWatcher(wayland::Server* server);

  WaylandWatcher(const WaylandWatcher&) = delete;
  WaylandWatcher& operator=(const WaylandWatcher&) = delete;

  ~WaylandWatcher() override;

  // Start/Stop watching the fd for testing.
  void StartForTesting();
  void StopForTesting();

 private:
  void Start();

  // base::MessagePumpForUI::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;

  void OnFileCanWriteWithoutBlocking(int fd) override;

  base::MessagePumpForUI::FdWatchController controller_;
  const raw_ptr<wayland::Server> server_;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_WATCHER_H_

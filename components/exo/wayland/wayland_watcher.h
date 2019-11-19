// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_WATCHER_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_WATCHER_H_

#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"

namespace exo {
namespace wayland {

class Server;

class WaylandWatcher : public base::MessagePumpForUI::FdWatcher {
 public:
  explicit WaylandWatcher(wayland::Server* server);
  ~WaylandWatcher() override;

 private:
  // base::MessagePumpForUI::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;

  void OnFileCanWriteWithoutBlocking(int fd) override;

  base::MessagePumpForUI::FdWatchController controller_;
  wayland::Server* const server_;

  DISALLOW_COPY_AND_ASSIGN(WaylandWatcher);
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_WATCHER_H_

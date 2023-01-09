// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_PROVIDER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace ash {
class DebugDaemonClient;

// This class hosts an instance of DebugDaemonClient used to connect to debugd
// using a private DBus connection. The private connection uses the sequence
// on which DebugDaemonClientProvider is instantiated as the origin thread. Use
// this class if you need to connect to debugd from non-UI thread.
//
// Example:
// // From non-UI thread. The private dbus::Bus binds the current sequence as
// // its origin thread.
// std::unique_ptr<DebugDaemonClientProvider> provider =
//     std::make_unique<DebugDaemonClientProvider>();
// DebugDaemonClient* client = provider->debug_daemon_client();
// client->GetPerfOutput(...);
//
class COMPONENT_EXPORT(DEBUG_DAEMON) DebugDaemonClientProvider {
 public:
  DebugDaemonClientProvider();

  DebugDaemonClientProvider(const DebugDaemonClientProvider&) = delete;
  DebugDaemonClientProvider& operator=(const DebugDaemonClientProvider&) =
      delete;

  ~DebugDaemonClientProvider();

  DebugDaemonClient* debug_daemon_client() const {
    return debug_daemon_client_.get();
  }

 private:
  // The private bus.
  scoped_refptr<dbus::Bus> dbus_bus_;
  scoped_refptr<base::SequencedTaskRunner> dbus_task_runner_;
  std::unique_ptr<DebugDaemonClient> debug_daemon_client_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_PROVIDER_H_

// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_PROVIDER_H_
#define CHROMEOS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_PROVIDER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {
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
  ~DebugDaemonClientProvider();

  DebugDaemonClient* debug_daemon_client() const {
    return debug_daemon_client_.get();
  }

 private:
  // The private bus.
  scoped_refptr<dbus::Bus> dbus_bus_;
  scoped_refptr<base::SequencedTaskRunner> dbus_task_runner_;
  std::unique_ptr<DebugDaemonClient> debug_daemon_client_;

  DISALLOW_COPY_AND_ASSIGN(DebugDaemonClientProvider);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_PROVIDER_H_

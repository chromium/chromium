// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/debug_daemon/debug_daemon_client_provider.h"

#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

DebugDaemonClientProvider::DebugDaemonClientProvider()
    : dbus_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})),
      debug_daemon_client_(DebugDaemonClient::Create()) {
  dbus::Bus::Options dbus_options;
  dbus_options.bus_type = dbus::Bus::SYSTEM;
  dbus_options.connection_type = dbus::Bus::PRIVATE;
  dbus_options.dbus_task_runner = dbus_task_runner_;
  dbus_bus_ = base::MakeRefCounted<dbus::Bus>(dbus_options);

  debug_daemon_client_->Init(dbus_bus_.get());
}

DebugDaemonClientProvider::~DebugDaemonClientProvider() {
  DCHECK(debug_daemon_client_);
  DCHECK(dbus_bus_);

  debug_daemon_client_ = nullptr;
  dbus_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, dbus_bus_));
}

}  // namespace chromeos

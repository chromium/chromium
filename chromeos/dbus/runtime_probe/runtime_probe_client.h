// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_RUNTIME_PROBE_RUNTIME_PROBE_CLIENT_H_
#define CHROMEOS_DBUS_RUNTIME_PROBE_RUNTIME_PROBE_CLIENT_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"
#include "chromeos/dbus/runtime_probe/runtime_probe.pb.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// RuntimeProbeClient is used to communicate with Runtime Probe, which provides
// data for hardware telemetry.
class COMPONENT_EXPORT(CHROMEOS_DBUS_RUNTIME_PROBE) RuntimeProbeClient
    : public DBusClient {
 public:
  using RuntimeProbeCallback = DBusMethodCallback<runtime_probe::ProbeResult>;

  // Creates an instance of RuntimeProbeClient.
  static std::unique_ptr<RuntimeProbeClient> Create();

  RuntimeProbeClient(const RuntimeProbeClient&) = delete;
  RuntimeProbeClient& operator=(const RuntimeProbeClient&) = delete;

  ~RuntimeProbeClient() override;

  // Probes categories.
  virtual void ProbeCategories(const runtime_probe::ProbeRequest& request,
                               RuntimeProbeCallback callback) = 0;

 protected:
  // Create() should be used instead.
  RuntimeProbeClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_RUNTIME_PROBE_RUNTIME_PROBE_CLIENT_H_

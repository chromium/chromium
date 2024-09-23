// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RUNTIME_PROBE_RUNTIME_PROBE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RUNTIME_PROBE_RUNTIME_PROBE_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/runtime_probe/runtime_probe.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/object_proxy.h"

namespace ash {

// RuntimeProbeClient is used to communicate with Runtime Probe, which provides
// data for hardware telemetry.
class COMPONENT_EXPORT(ASH_DBUS_RUNTIME_PROBE) RuntimeProbeClient
    : public chromeos::DBusClient {
 public:
  using RuntimeProbeCallback =
      chromeos::DBusMethodCallback<runtime_probe::ProbeResult>;

  // Returns the global instance if initialized. May return null.
  static RuntimeProbeClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  RuntimeProbeClient(const RuntimeProbeClient&) = delete;
  RuntimeProbeClient& operator=(const RuntimeProbeClient&) = delete;

  // Probes categories.
  virtual void ProbeCategories(const runtime_probe::ProbeRequest& request,
                               RuntimeProbeCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  RuntimeProbeClient();
  ~RuntimeProbeClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RUNTIME_PROBE_RUNTIME_PROBE_CLIENT_H_

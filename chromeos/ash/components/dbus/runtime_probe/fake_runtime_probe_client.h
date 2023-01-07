// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RUNTIME_PROBE_FAKE_RUNTIME_PROBE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RUNTIME_PROBE_FAKE_RUNTIME_PROBE_CLIENT_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/runtime_probe/runtime_probe_client.h"

namespace ash {

// FakeRuntimeProbeClient is a light mock of RuntimeProbeClient used for
// used for tests and when running ChromeOS build on Linux.
class COMPONENT_EXPORT(ASH_DBUS_RUNTIME_PROBE) FakeRuntimeProbeClient
    : public RuntimeProbeClient {
 public:
  FakeRuntimeProbeClient();

  FakeRuntimeProbeClient(const FakeRuntimeProbeClient&) = delete;
  FakeRuntimeProbeClient& operator=(const FakeRuntimeProbeClient&) = delete;

  ~FakeRuntimeProbeClient() override;

  // RuntimeProbeClient overrides:
  void Init(dbus::Bus* bus) override {}
  void ProbeCategories(const runtime_probe::ProbeRequest& request,
                       RuntimeProbeCallback callback) override;

 private:
  // Used to simulates changes in live values. This field will be iterated
  // in small range and live values will be adjusted proportional to this
  // value.
  int live_offset_ = 0;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeRuntimeProbeClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RUNTIME_PROBE_FAKE_RUNTIME_PROBE_CLIENT_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_RUNTIME_PROBE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_RUNTIME_PROBE_CLIENT_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/runtime_probe_client.h"

namespace chromeos {

// FakeRuntimeProbeClient is a light mock of RuntimeProbeClient used for
// used for tests and when running ChromeOS build on Linux.
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeRuntimeProbeClient
    : public RuntimeProbeClient {
 public:
  FakeRuntimeProbeClient();
  ~FakeRuntimeProbeClient() override;

  // RuntimeProbeClient overrides:
  void ProbeCategories(const runtime_probe::ProbeRequest& request,
                       RuntimeProbeCallback callback) override;

 protected:
  void Init(dbus::Bus* bus) override {}

 private:
  // Used to simulates changes in live values. This field will be iterated
  // in small range and live values will be adjusted proportional to this
  // value.
  int live_offset_ = 0;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeRuntimeProbeClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeRuntimeProbeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_RUNTIME_PROBE_CLIENT_H_

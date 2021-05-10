// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_OOBE_CONFIGURATION_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_OOBE_CONFIGURATION_CLIENT_H_

#include "base/macros.h"
#include "chromeos/dbus/oobe_configuration_client.h"

namespace chromeos {

// A fake implementation of OobeConfigurationClient, provides configuration
// specified via command-line flag.
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeOobeConfigurationClient
    : public OobeConfigurationClient {
 public:
  FakeOobeConfigurationClient();
  ~FakeOobeConfigurationClient() override;

  void Init(dbus::Bus* bus) override;

  // EasyUnlockClient overrides
  void CheckForOobeConfiguration(ConfigurationCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeOobeConfigurationClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_OOBE_CONFIGURATION_CLIENT_H_

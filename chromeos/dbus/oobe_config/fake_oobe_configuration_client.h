// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_OOBE_CONFIG_FAKE_OOBE_CONFIGURATION_CLIENT_H_
#define CHROMEOS_DBUS_OOBE_CONFIG_FAKE_OOBE_CONFIGURATION_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/dbus/oobe_config/oobe_configuration_client.h"

namespace chromeos {

// A fake implementation of OobeConfigurationClient, provides configuration
// specified via command-line flag.
class COMPONENT_EXPORT(CHROMEOS_DBUS_OOBE_CONFIG) FakeOobeConfigurationClient
    : public OobeConfigurationClient {
 public:
  FakeOobeConfigurationClient();

  FakeOobeConfigurationClient(const FakeOobeConfigurationClient&) = delete;
  FakeOobeConfigurationClient& operator=(const FakeOobeConfigurationClient&) =
      delete;

  ~FakeOobeConfigurationClient() override;

  void Init(dbus::Bus* bus) override;

  // EasyUnlockClient overrides
  void CheckForOobeConfiguration(ConfigurationCallback callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_OOBE_CONFIG_FAKE_OOBE_CONFIGURATION_CLIENT_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_OOBE_CONFIGURATION_CLIENT_H_
#define CHROMEOS_DBUS_OOBE_CONFIGURATION_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"

namespace chromeos {

// Client for calling OobeConfiguration dbus service. The service provides
// verified OOBE configuration, that allows to automate out-of-box experience.
// This configuration comes either from the state before power wash, or from
// USB stick during USB-based enrollment flow.

class CHROMEOS_EXPORT OobeConfigurationClient : public DBusClient {
 public:
  using ConfigurationCallback =
      base::OnceCallback<void(bool has_configuration,
                              const std::string& configuration)>;

  ~OobeConfigurationClient() override = default;

  // Factory function.
  static std::unique_ptr<OobeConfigurationClient> Create();

  // Checks if valid OOBE configuration exists.
  virtual void CheckForOobeConfiguration(ConfigurationCallback callback) = 0;

 protected:
  friend class OobeConfigurationClientTest;

  // Create() should be used instead.
  OobeConfigurationClient() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeConfigurationClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_OOBE_CONFIGURATION_CLIENT_H_

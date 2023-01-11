// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_OOBE_CONFIG_OOBE_CONFIGURATION_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_OOBE_CONFIG_OOBE_CONFIGURATION_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// Client for calling OobeConfiguration dbus service. The service provides
// verified OOBE configuration, that allows to automate out-of-box experience.
// This configuration comes either from the state before power wash, or from
// USB stick during USB-based enrollment flow.
class COMPONENT_EXPORT(ASH_DBUS_OOBE_CONFIG) OobeConfigurationClient
    : public chromeos::DBusClient {
 public:
  using ConfigurationCallback =
      base::OnceCallback<void(bool has_configuration,
                              const std::string& configuration)>;

  // Returns the global instance if initialized. May return null.
  static OobeConfigurationClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  OobeConfigurationClient(const OobeConfigurationClient&) = delete;
  OobeConfigurationClient& operator=(const OobeConfigurationClient&) = delete;

  // Checks if valid OOBE configuration exists.
  virtual void CheckForOobeConfiguration(ConfigurationCallback callback) = 0;

 protected:
  friend class OobeConfigurationClientTest;

  // Initialize() should be used instead.
  OobeConfigurationClient();
  ~OobeConfigurationClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_OOBE_CONFIG_OOBE_CONFIGURATION_CLIENT_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_OOBE_CONFIG_OOBE_CONFIGURATION_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_OOBE_CONFIG_OOBE_CONFIGURATION_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_config.pb.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// Client for calling OobeConfiguration dbus service. The service provides
// verified OOBE configuration, that allows to automate out-of-box experience.
// This configuration comes either from the state before power wash (for
// enterprise rollback), or was written to the ChromeOS Flex image prior
// to installation for Flex Auto Enrollment.
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

  // Clears the Flex config from disk.
  virtual void DeleteFlexOobeConfig() = 0;

 protected:
  friend class OobeConfigurationClientTest;

  // Initialize() should be used instead.
  OobeConfigurationClient();
  ~OobeConfigurationClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_OOBE_CONFIG_OOBE_CONFIGURATION_CLIENT_H_

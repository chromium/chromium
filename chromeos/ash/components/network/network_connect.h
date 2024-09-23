// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONNECT_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONNECT_H_

#include <string>

#include "base/component_export.h"
#include "base/values.h"

namespace ash {

// NetworkConnect is a state machine designed to handle the complex UI flows
// associated with connecting to a network (and related tasks). Any showing
// of UI is handled by the NetworkConnect::Delegate implementation.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkConnect {
 public:
  // Track the source of NetworkConnect calls for metrics.
  enum class Source {
    // Opened from the Settings UI.
    kSettings = 1,
    // Opened from the QuickSettings UI.
    kQuickSettings = 2,
  };

  class COMPONENT_EXPORT(CHROMEOS_NETWORK) Delegate {
   public:
    // Shows UI to configure or activate the network specified by |network_id|,
    // which may include showing Payment or Portal UI when appropriate.
    virtual void ShowNetworkConfigure(const std::string& network_id) = 0;

    // Shows the settings related to network. If |network_id| is not empty,
    // show the settings for that network.
    virtual void ShowNetworkSettings(const std::string& network_id) = 0;

    // Shows UI to enroll the network specified by |network_id| if appropriate
    // and returns true, otherwise returns false.
    virtual bool ShowEnrollNetwork(const std::string& network_id) = 0;

    // Shows UI to setup a mobile network.
    virtual void ShowMobileSetupDialog(const std::string& network_id) = 0;

    // Shows UI displaying carrier network account details.
    virtual void ShowCarrierAccountDetail(const std::string& network_id) = 0;

    // Shows UI displaying carrier unlock notification.
    virtual void ShowCarrierUnlockNotification() = 0;

    // Shows portal signin.
    virtual void ShowPortalSignin(const std::string& network_id,
                                  Source source) = 0;

    // Shows an error notification. |error_name| is an error defined in
    // NetworkConnectionHandler. |network_id| may be empty.
    virtual void ShowNetworkConnectError(const std::string& error_name,
                                         const std::string& network_id) = 0;

    // Shows an error notification during mobile activation.
    virtual void ShowMobileActivationError(const std::string& network_id) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Creates the global NetworkConnect object. |delegate| is owned by the
  // caller.
  static void Initialize(Delegate* delegate);

  // Destroys the global NetworkConnect object.
  static void Shutdown();

  // Returns true if the global NetworkConnect object is initialized.
  static bool IsInitialized();

  // Returns the global NetworkConnect object if initialized or null.
  static NetworkConnect* Get();

  NetworkConnect(const NetworkConnect&) = delete;
  NetworkConnect& operator=(const NetworkConnect&) = delete;

  virtual ~NetworkConnect();

  // Requests a network connection and handles any errors and notifications.
  virtual void ConnectToNetworkId(const std::string& network_id) = 0;

  // Requests a network disconnection. Ignores any errors and notifications.
  virtual void DisconnectFromNetworkId(const std::string& network_id) = 0;

  // Determines whether or not a network requires a connection to activate or
  // setup and either shows a notification or opens the mobile setup dialog.
  virtual void ShowMobileSetup(const std::string& network_id) = 0;

  // Opens the carrier account detail page.
  virtual void ShowCarrierAccountDetail(const std::string& network_id) = 0;

  // Shows carrier unlock notification.
  virtual void ShowCarrierUnlockNotification() = 0;

  // Opens the portal signin.
  virtual void ShowPortalSignin(const std::string& network_id,
                                Source source) = 0;

  // Configures a network with a dictionary of Shill properties, then sends a
  // connect request. The profile is set according to 'shared' if allowed.
  // TODO(stevenjb): Use ONC properties instead of shill.
  virtual void ConfigureNetworkIdAndConnect(
      const std::string& network_id,
      const base::Value::Dict& shill_properties,
      bool shared) = 0;

  // Requests a new network configuration to be created from a dictionary of
  // Shill properties and sends a connect request if the configuration succeeds.
  // The profile used is determined by |shared|.
  // TODO(stevenjb): Use ONC properties instead of shill.
  virtual void CreateConfigurationAndConnect(base::Value::Dict shill_properties,
                                             bool shared) = 0;

  // Requests a new network configuration to be created from a dictionary of
  // Shill properties. The profile used is determined by |shared|.
  virtual void CreateConfiguration(base::Value::Dict shill_properties,
                                   bool shared) = 0;

 protected:
  NetworkConnect();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONNECT_H_

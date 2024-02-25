// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_PROFILE_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_PROFILE_HANDLER_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile.h"

namespace ash {

class NetworkProfileObserver;

class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkProfileHandler
    : public ShillPropertyChangedObserver {
 public:
  typedef std::vector<NetworkProfile> ProfileList;

  NetworkProfileHandler(const NetworkProfileHandler&) = delete;
  NetworkProfileHandler& operator=(const NetworkProfileHandler&) = delete;

  ~NetworkProfileHandler() override;

  void AddObserver(NetworkProfileObserver* observer);
  void RemoveObserver(NetworkProfileObserver* observer);
  bool HasObserver(NetworkProfileObserver* observer);

  void GetManagerPropertiesCallback(
      std::optional<base::Value::Dict> properties);

  // ShillPropertyChangedObserver overrides
  void OnPropertyChanged(const std::string& name,
                         const base::Value& value) override;

  void GetProfilePropertiesCallback(const std::string& profile_path,
                                    base::Value::Dict properties);

  const NetworkProfile* GetProfileForPath(
      const std::string& profile_path) const;
  const NetworkProfile* GetProfileForUserhash(
      const std::string& userhash) const;

  // Returns the first profile entry with a non-empty userhash.
  // TODO(stevenjb): Replace with GetProfileForUserhash() with the correct
  // userhash.
  const NetworkProfile* GetDefaultUserProfile() const;

  // Fetch the always-on VPN settings from |profile_path| profile.
  // |callback| is called with the always-on VPN mode and service path.
  void GetAlwaysOnVpnConfiguration(
      const std::string& profile_path,
      base::OnceCallback<void(std::string, std::string)> callback);

  // Sets the always-on VPN mode |mode| in |profile_path| profile.
  void SetAlwaysOnVpnMode(const std::string& profile_path,
                          const std::string& mode);

  // Sets the always-on VPN service in |profile_path| profile.
  void SetAlwaysOnVpnService(const std::string& profile_path,
                             const std::string& service_path);

  static std::string GetSharedProfilePath();

  static std::unique_ptr<NetworkProfileHandler> InitializeForTesting();

 protected:
  friend class AutoConnectHandlerTest;
  friend class ClientCertResolverTest;
  friend class NetworkConnectionHandlerImplTest;
  friend class NetworkHandler;
  friend class ProhibitedTechnologiesHandlerTest;
  NetworkProfileHandler();

  // Add ShillManagerClient property observer and request initial list.
  void Init();

  void AddProfile(const NetworkProfile& profile);
  void RemoveProfile(const std::string& profile_path);

 private:
  // Callback for always-on VPN configuration trigger when a result for the
  // GetAlwaysOnVpnConfiguration() call is available. It extracts the two
  // settings and transmit them to the original caller through |callback|.
  void GetAlwaysOnVpnConfigurationCallback(
      base::OnceCallback<void(std::string, std::string)> callback,
      base::Value::Dict properties);

  ProfileList profiles_;

  // Contains the profile paths for which properties were requested. Once the
  // properties are retrieved and the path is still in this set, a new profile
  // object is created.
  std::set<std::string> pending_profile_creations_;
  base::ObserverList<NetworkProfileObserver, true>::Unchecked observers_;

  // For Shill client callbacks
  base::WeakPtrFactory<NetworkProfileHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_PROFILE_HANDLER_H_

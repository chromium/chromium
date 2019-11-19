// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_PROFILE_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_PROFILE_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_profile.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class NetworkProfileObserver;

class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkProfileHandler
    : public ShillPropertyChangedObserver {
 public:
  typedef std::vector<NetworkProfile> ProfileList;

  ~NetworkProfileHandler() override;

  void AddObserver(NetworkProfileObserver* observer);
  void RemoveObserver(NetworkProfileObserver* observer);

  void GetManagerPropertiesCallback(DBusMethodCallStatus call_status,
                                    const base::DictionaryValue& properties);

  // ShillPropertyChangedObserver overrides
  void OnPropertyChanged(const std::string& name,
                         const base::Value& value) override;

  void GetProfilePropertiesCallback(const std::string& profile_path,
                                    const base::DictionaryValue& properties);

  const NetworkProfile* GetProfileForPath(
      const std::string& profile_path) const;
  const NetworkProfile* GetProfileForUserhash(
      const std::string& userhash) const;

  // Returns the first profile entry with a non-empty userhash.
  // TODO(stevenjb): Replace with GetProfileForUserhash() with the correct
  // userhash.
  const NetworkProfile* GetDefaultUserProfile() const;

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
  ProfileList profiles_;

  // Contains the profile paths for which properties were requested. Once the
  // properties are retrieved and the path is still in this set, a new profile
  // object is created.
  std::set<std::string> pending_profile_creations_;
  base::ObserverList<NetworkProfileObserver, true>::Unchecked observers_;

  // For Shill client callbacks
  base::WeakPtrFactory<NetworkProfileHandler> weak_ptr_factory_{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkProfileHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_PROFILE_HANDLER_H_

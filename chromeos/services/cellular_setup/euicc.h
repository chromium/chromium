// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_EUICC_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_EUICC_H_

#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace chromeos {
namespace cellular_setup {

class ESimProfile;
class ESimManager;

// Implementation of mojom::Euicc. This class represents an EUICC hardware
// available on the device. Euicc holds multiple ESimProfile instances.
class Euicc : public mojom::Euicc {
 public:
  Euicc(const dbus::ObjectPath& path, ESimManager* esim_manager);
  Euicc(const Euicc&) = delete;
  Euicc& operator=(const Euicc&) = delete;
  ~Euicc() override;

  // mojom::Euicc:
  void GetProperties(GetPropertiesCallback callback) override;
  void GetProfileList(GetProfileListCallback callback) override;
  void InstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallProfileFromActivationCodeCallback callback) override;
  void RequestPendingProfiles(RequestPendingProfilesCallback callback) override;

  // Updates list of eSIM profiles for this euicc from D-Bus.
  void UpdateProfileList();

  // Updates properties for this Euicc from D-Bus.
  void UpdateProperties();

  // Returns a new pending remote attached to this instance.
  mojo::PendingRemote<mojom::Euicc> CreateRemote();

  // Returns ESimProfile instance under this Euicc with given path.
  ESimProfile* GetProfileFromPath(const dbus::ObjectPath& path);

  const dbus::ObjectPath& path() { return path_; }
  const mojom::EuiccPropertiesPtr& properties() { return properties_; }

 private:
  // Type of callback for profile installation methods.
  using ProfileInstallResultCallback =
      base::OnceCallback<void(mojom::ProfileInstallResult)>;

  void OnProfileInstallResult(InstallProfileFromActivationCodeCallback callback,
                              HermesResponseStatus status,
                              const dbus::ObjectPath* object_path);
  void OnRequestPendingEventsResult(RequestPendingProfilesCallback callback,
                                    HermesResponseStatus status);
  mojom::ProfileInstallResult GetPendingProfileInfoFromActivationCode(
      const std::string& activation_code,
      ESimProfile** profile_info);
  ESimProfile* GetOrCreateESimProfile(
      const dbus::ObjectPath& carrier_profile_path);
  void RemoveUntrackedProfiles(
      const std::set<dbus::ObjectPath>& new_profile_paths);

  // Reference to ESimManager that owns this Euicc.
  ESimManager* esim_manager_;
  mojo::ReceiverSet<mojom::Euicc> receiver_set_;
  mojom::EuiccPropertiesPtr properties_;
  dbus::ObjectPath path_;
  std::vector<std::unique_ptr<ESimProfile>> esim_profiles_;

  base::WeakPtrFactory<Euicc> weak_ptr_factory_{this};
};

}  // namespace cellular_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_EUICC_H_
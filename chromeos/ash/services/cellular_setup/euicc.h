// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_EUICC_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_EUICC_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace dbus {
class ObjectPath;
}

namespace ash {

class CellularESimProfile;

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
      mojom::ProfileInstallMethod install_method,
      InstallProfileFromActivationCodeCallback callback) override;
  void RequestAvailableProfiles(
      RequestAvailableProfilesCallback callback) override;
  void RefreshInstalledProfiles(
      RefreshInstalledProfilesCallback callback) override;
  void GetEidQRCode(GetEidQRCodeCallback callback) override;

  // Updates list of eSIM profiles for this euicc from with the given
  // |esim_profile_states|.
  void UpdateProfileList(
      const std::vector<CellularESimProfile>& esim_profile_states);

  // Updates properties for this Euicc from D-Bus.
  void UpdateProperties();

  // Returns a new pending remote attached to this instance.
  mojo::PendingRemote<mojom::Euicc> CreateRemote();

  // Returns ESimProfile instance under this Euicc with given path.
  ESimProfile* GetProfileFromPath(const dbus::ObjectPath& path);

  const dbus::ObjectPath& path() { return path_; }
  const mojom::EuiccPropertiesPtr& properties() { return properties_; }

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RequestPendingProfilesResult {
    kSuccess = 0,
    kInhibitFailed = 1,
    kHermesRequestFailed = 2,
    kMaxValue = kHermesRequestFailed
  };
  static void RecordRequestPendingProfilesResult(
      RequestPendingProfilesResult result);

  void OnESimInstallProfileResult(
      InstallProfileFromActivationCodeCallback callback,
      HermesResponseStatus hermes_status,
      std::optional<dbus::ObjectPath> profile_path,
      std::optional<std::string> service_path);
  void OnRequestAvailableProfiles(
      RequestAvailableProfilesCallback callback,
      mojom::ESimOperationResult result,
      std::vector<CellularESimProfile> profile_list);
  // Updates an ESimProfile in |esim_profiles_| with values from given
  // |esim_profile_state| or creates new one if it doesn't exist. Returns
  // pointer to ESimProfile object if one was created.
  ESimProfile* UpdateOrCreateESimProfile(
      const CellularESimProfile& esim_profile_state);
  // Removes any ESimProfile object in |esim_profiles_| that doesn't exists in
  // given |esim_profile_states|. Returns true if any profiles were removed.
  bool RemoveUntrackedProfiles(
      const std::vector<CellularESimProfile>& esim_profile_states);

  // Reference to ESimManager that owns this Euicc.
  raw_ptr<ESimManager> esim_manager_;
  mojo::ReceiverSet<mojom::Euicc> receiver_set_;
  mojom::EuiccPropertiesPtr properties_;
  dbus::ObjectPath path_;
  std::vector<std::unique_ptr<ESimProfile>> esim_profiles_;

  // Maps profile dbus paths to InstallProfileFromActivation method callbacks
  // that are pending creation of a new ESimProfile object.
  std::map<dbus::ObjectPath, InstallProfileFromActivationCodeCallback>
      install_calls_pending_create_;

  base::WeakPtrFactory<Euicc> weak_ptr_factory_{this};
};

}  // namespace cellular_setup
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_EUICC_H_

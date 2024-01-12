// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_ESIM_PROFILE_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_ESIM_PROFILE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace dbus {
class ObjectPath;
}

namespace ash {

class CellularESimProfile;

namespace cellular_setup {

class Euicc;
class ESimManager;

// Implementation of mojom::ESimProfile. This class represents an
// eSIM profile installed on an EUICC.
class ESimProfile : public mojom::ESimProfile {
 public:
  ESimProfile(const CellularESimProfile& esim_profile_state,
              Euicc* euicc,
              ESimManager* esim_manager);
  ESimProfile(const ESimProfile&) = delete;
  ESimProfile& operator=(const ESimProfile&) = delete;
  ~ESimProfile() override;

  // mojom::ESimProfile:
  void GetProperties(GetPropertiesCallback callback) override;
  void InstallProfile(const std::string& confirmation_code,
                      InstallProfileCallback callback) override;
  void UninstallProfile(UninstallProfileCallback callback) override;
  void SetProfileNickname(const std::u16string& nickname,
                          SetProfileNicknameCallback callback) override;

  // Update properties for this ESimProfile from D-Bus.
  void UpdateProperties(const CellularESimProfile& esim_profile_state,
                        bool notify);

  // Called before profile is removed from the euicc.
  void OnProfileRemove();

  // Returns a new pending remote attached to this instance.
  mojo::PendingRemote<mojom::ESimProfile> CreateRemote();

  const dbus::ObjectPath& path() { return path_; }
  const mojom::ESimProfilePropertiesPtr& properties() { return properties_; }

 private:
  // Type of callback for profile installation methods.
  using ProfileInstallResultCallback =
      base::OnceCallback<void(mojom::ProfileInstallResult)>;

  // Type of callback to be passed to EnsureProfileExists method. The callback
  // receives a boolean indicating request profile succeess status and inhibit
  // lock that was passed to the method.
  using EnsureProfileExistsOnEuiccCallback =
      base::OnceCallback<void(bool,
                              std::unique_ptr<CellularInhibitor::InhibitLock>)>;

  void EnsureProfileExistsOnEuicc(
      EnsureProfileExistsOnEuiccCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnRequestInstalledProfiles(
      EnsureProfileExistsOnEuiccCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnRefreshSmdxProfiles(
      EnsureProfileExistsOnEuiccCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      HermesResponseStatus status,
      const std::vector<dbus::ObjectPath>& profile_paths);
  void OnRequestPendingProfiles(
      EnsureProfileExistsOnEuiccCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      HermesResponseStatus status);
  void OnRequestProfiles(
      EnsureProfileExistsOnEuiccCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      bool success);
  void PerformInstallProfile(
      const std::string& confirmation_code,
      bool request_profile_success,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void PerformSetProfileNickname(
      const std::u16string& nickname,
      bool request_profile_success,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnPendingProfileInstallResult(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      HermesResponseStatus status);
  void OnNewProfileEnableSuccess(const std::string& service_path,
                                 bool auto_connected);
  void OnPrepareCellularNetworkForConnectionFailure(
      const std::string& service_path,
      const std::string& error_name);
  void OnProfileUninstallResult(bool success);
  void OnProfileNicknameSet(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      HermesResponseStatus status);
  bool ProfileExistsOnEuicc();
  bool IsProfileInstalled();
  bool IsProfileManaged();

  // Reference to Euicc that owns this profile.
  raw_ptr<Euicc> euicc_;
  // Reference to ESimManager that owns Euicc of this profile.
  raw_ptr<ESimManager> esim_manager_;
  UninstallProfileCallback uninstall_callback_;
  SetProfileNicknameCallback set_profile_nickname_callback_;
  InstallProfileCallback install_callback_;
  mojo::ReceiverSet<mojom::ESimProfile> receiver_set_;
  mojom::ESimProfilePropertiesPtr properties_;
  dbus::ObjectPath path_;
  base::WeakPtrFactory<ESimProfile> weak_ptr_factory_{this};
};

}  // namespace cellular_setup
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_ESIM_PROFILE_H_

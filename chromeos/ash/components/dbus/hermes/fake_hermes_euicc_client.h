// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_FAKE_HERMES_EUICC_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_FAKE_HERMES_EUICC_CLIENT_H_

#include <queue>

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash {

// Fake implementation for HermesEuiccClient. This class also interacts with
// fake shill clients to setup fake cellular services corresponding to eSIM
// profiles.
class COMPONENT_EXPORT(HERMES_CLIENT) FakeHermesEuiccClient
    : public HermesEuiccClient,
      public HermesEuiccClient::TestInterface {
 public:
  class Properties : public HermesEuiccClient::Properties {
   public:
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    // dbus::PropertySet:
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override;
    void GetAll() override;
    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override;
  };

  FakeHermesEuiccClient();
  FakeHermesEuiccClient(const FakeHermesEuiccClient&) = delete;
  FakeHermesEuiccClient& operator=(const FakeHermesEuiccClient&) = delete;
  ~FakeHermesEuiccClient() override;

  // HermesEuiccClient::TestInterface:
  void ClearEuicc(const dbus::ObjectPath& euicc_path) override;
  void ResetPendingEventsRequested() override;
  dbus::ObjectPath AddFakeCarrierProfile(
      const dbus::ObjectPath& euicc_path,
      hermes::profile::State state,
      const std::string& activation_code,
      AddCarrierProfileBehavior add_carrier_profile_behavior) override;
  void AddCarrierProfile(
      const dbus::ObjectPath& path,
      const dbus::ObjectPath& euicc_path,
      const std::string& iccid,
      const std::string& name,
      const std::string& nickname,
      const std::string& service_provider,
      const std::string& activation_code,
      const std::string& network_service_path,
      hermes::profile::State state,
      hermes::profile::ProfileClass profile_class,
      AddCarrierProfileBehavior add_carrier_profile_behavior) override;
  bool RemoveCarrierProfile(
      const dbus::ObjectPath& euicc_path,
      const dbus::ObjectPath& carrier_profile_path) override;
  void UpdateShillDeviceSimSlotInfo() override;
  void QueueHermesErrorStatus(HermesResponseStatus status) override;
  void SetNextInstallProfileFromActivationCodeResult(
      HermesResponseStatus status) override;
  void SetNextRefreshSmdxProfilesResult(
      std::vector<dbus::ObjectPath> profiles) override;
  void SetInteractiveDelay(base::TimeDelta delay) override;
  std::string GenerateFakeActivationCode() override;
  std::string GetDBusErrorActivationCode() override;
  bool GetLastRefreshProfilesRestoreSlotArg() override;

  // HermesEuiccClient:
  void InstallProfileFromActivationCode(
      const dbus::ObjectPath& euicc_path,
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallCarrierProfileCallback callback) override;
  void InstallPendingProfile(const dbus::ObjectPath& euicc_path,
                             const dbus::ObjectPath& carrier_profile_path,
                             const std::string& confirmation_code,
                             HermesResponseCallback callback) override;
  void RefreshInstalledProfiles(const dbus::ObjectPath& euicc_path,
                                bool restore_slot,
                                HermesResponseCallback callback) override;
  void RefreshSmdxProfiles(const dbus::ObjectPath& euicc_path,
                           const std::string& activation_code,
                           bool restore_slot,
                           RefreshSmdxProfilesCallback callback) override;
  void RequestPendingProfiles(const dbus::ObjectPath& euicc_path,
                              const std::string& root_smds,
                              HermesResponseCallback callback) override;
  void UninstallProfile(const dbus::ObjectPath& euicc_path,
                        const dbus::ObjectPath& carrier_profile_path,
                        HermesResponseCallback callback) override;
  void ResetMemory(const dbus::ObjectPath& euicc_path,
                   hermes::euicc::ResetOptions reset_option,
                   HermesResponseCallback callback) override;
  Properties* GetProperties(const dbus::ObjectPath& euicc_path) override;
  HermesEuiccClient::TestInterface* GetTestInterface() override;

 private:
  void DoInstallProfileFromActivationCode(
      const dbus::ObjectPath& euicc_path,
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallCarrierProfileCallback callback);
  void DoInstallPendingProfile(const dbus::ObjectPath& euicc_path,
                               const dbus::ObjectPath& carrier_profile_path,
                               const std::string& confirmation_code,
                               HermesResponseCallback callback);
  void DoRequestInstalledProfiles(const dbus::ObjectPath& euicc_path,
                                  HermesResponseCallback callback);
  void DoRefreshSmdxProfiles(const dbus::ObjectPath& euicc_path,
                             const std::string& activation_code,
                             RefreshSmdxProfilesCallback callback);
  void DoRequestPendingProfiles(const dbus::ObjectPath& euicc_path,
                                HermesResponseCallback callback);
  void DoUninstallProfile(const dbus::ObjectPath& euicc_path,
                          const dbus::ObjectPath& carrier_profile_path,
                          HermesResponseCallback callback);
  void DoResetMemory(const dbus::ObjectPath& euicc_path,
                     hermes::euicc::ResetOptions reset_option,
                     HermesResponseCallback callback);
  dbus::ObjectPath AddFakeCarrierProfile(hermes::profile::State state,
                                         std::string activation_code);
  void CreateCellularService(const dbus::ObjectPath& euicc_path,
                             const dbus::ObjectPath& carrier_profile_path);
  // Add the default cellular APN. This is intended to simulate the
  // auto-detecting APN in Shill.
  void CreateDefaultModbApn(const std::string& service_path);
  void CallNotifyPropertyChanged(const dbus::ObjectPath& object_path,
                                 const std::string& property_name);
  void NotifyPropertyChanged(const dbus::ObjectPath& object_path,
                             const std::string& property_name);
  void QueueInstalledProfile(const dbus::ObjectPath& euicc_path,
                             const dbus::ObjectPath& profile_path);

  // Indicates whether a pending event request has already been made.
  bool pending_event_requested_ = false;

  // Counter to generate fake ids and properties for profiles.
  int fake_profile_counter_ = 0;

  // When set, this will be returned as the result of the next attempt to
  // install a profile using an activation code.
  std::optional<HermesResponseStatus> next_install_profile_result_;

  // When set, this will be returned as the result the next time that we refresh
  // the available SM-DX profiles.
  std::optional<std::vector<dbus::ObjectPath>>
      next_refresh_smdx_profiles_result_;

  // Queue of error code to be returned from method calls.
  std::queue<HermesResponseStatus> error_status_queue_;

  // Mapping between carrier profile objects and their corresponding
  // shill network service paths.
  std::map<dbus::ObjectPath, std::string> profile_service_path_map_;

  // Delay for simulating slow methods.
  base::TimeDelta interactive_delay_;

  // The |restore_slot| argument for the last call to
  // RefreshInstalledProfiles.
  bool last_restore_slot_arg_ = false;

  using InstalledProfileQueue = std::queue<dbus::ObjectPath>;
  using InstalledProfileQueueMap =
      std::map<const dbus::ObjectPath, std::unique_ptr<InstalledProfileQueue>>;
  // Queue of installed profiles paths for which a network service
  // has been created, but listing in Euicc is pending a call to
  // RequestInstalledProfiles.
  InstalledProfileQueueMap installed_profile_queue_map_;

  using PropertiesMap =
      std::map<const dbus::ObjectPath, std::unique_ptr<Properties>>;
  PropertiesMap properties_map_;

  base::WeakPtrFactory<FakeHermesEuiccClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_FAKE_HERMES_EUICC_CLIENT_H_

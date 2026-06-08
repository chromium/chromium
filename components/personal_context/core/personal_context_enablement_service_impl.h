// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_IMPL_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/account_settings/account_setting_service.h"
#include "components/personal_context/core/country_type.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefService;

namespace personal_context {

class PersonalContextEnablementServiceImpl
    : public PersonalContextEnablementService,
      public signin::IdentityManager::Observer,
      public account_settings::AccountSettingService::Observer {
 public:
  explicit PersonalContextEnablementServiceImpl(
      account_settings::AccountSettingService* account_settings_service,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      GeoIpCountryCode country_code,
      std::string locale);
  PersonalContextEnablementServiceImpl(
      const PersonalContextEnablementServiceImpl&) = delete;
  PersonalContextEnablementServiceImpl& operator=(
      const PersonalContextEnablementServiceImpl&) = delete;
  ~PersonalContextEnablementServiceImpl() override;

  // PersonalContextEnablementService:
  void AddObserver(
      PersonalContextEnablementService::Observer* observer) override;
  void RemoveObserver(
      PersonalContextEnablementService::Observer* observer) override;
  PersonalContextEnablementState GetEnablementState() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  // account_settings::AccountSettingService::Observer:
  void OnAccountSettingDataUpdated(const std::string& setting_name) override;

 private:
  friend class PersonalContextEnablementServiceImplTestApi;

  PersonalContextEnablementState ComputeEnablementState();
  void UpdateEnablementState();

  const raw_ptr<account_settings::AccountSettingService>
      account_settings_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<PrefService> pref_service_;
  const GeoIpCountryCode country_code_;
  const std::string locale_;
  base::ObserverList<PersonalContextEnablementService::Observer> observers_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};
  base::ScopedObservation<account_settings::AccountSettingService,
                          account_settings::AccountSettingService::Observer>
      account_settings_observation_{this};
  PrefChangeRegistrar pref_registrar_;
  // Cached last enablement state.
  PersonalContextEnablementState enablement_state_ =
      PersonalContextEnablementState::kDisabledNotEligible;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_IMPL_H_

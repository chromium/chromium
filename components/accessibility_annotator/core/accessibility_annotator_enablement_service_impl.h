// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service.h"
#include "components/accessibility_annotator/core/country_type.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"

namespace account_settings {
class AccountSettingService;
}  // namespace account_settings

class PrefService;

namespace accessibility_annotator {

class AccessibilityAnnotatorEnablementServiceImpl
    : public AccessibilityAnnotatorEnablementService,
      public signin::IdentityManager::Observer,
      public subscription_eligibility::SubscriptionEligibilityService::
          Observer {
 public:
  explicit AccessibilityAnnotatorEnablementServiceImpl(
      account_settings::AccountSettingService* account_settings_service,
      signin::IdentityManager* identity_manager,
      subscription_eligibility::SubscriptionEligibilityService*
          subscription_eligibility_service,
      PrefService* pref_service,
      GeoIpCountryCode country_code);
  AccessibilityAnnotatorEnablementServiceImpl(
      const AccessibilityAnnotatorEnablementServiceImpl&) = delete;
  AccessibilityAnnotatorEnablementServiceImpl& operator=(
      const AccessibilityAnnotatorEnablementServiceImpl&) = delete;
  ~AccessibilityAnnotatorEnablementServiceImpl() override;

  // AccessibilityAnnotatorEnablementService:
  void AddObserver(
      AccessibilityAnnotatorEnablementService::Observer* observer) override;
  void RemoveObserver(
      AccessibilityAnnotatorEnablementService::Observer* observer) override;
  RemoteAnnotatorEnablementState GetEnablementState() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // subscription_eligibility::SubscriptionEligibilityService::Observer:
  void OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) override;

 private:
  friend class AccessibilityAnnotatorEnablementServiceImplTestApi;

  RemoteAnnotatorEnablementState ComputeEnablementState();
  void UpdateEnablementState();

  const raw_ptr<account_settings::AccountSettingService>
      account_settings_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<subscription_eligibility::SubscriptionEligibilityService>
      subscription_eligibility_service_;
  const raw_ptr<PrefService> pref_service_;
  const GeoIpCountryCode country_code_;
  base::ObserverList<AccessibilityAnnotatorEnablementService::Observer>
      observers_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};
  base::ScopedObservation<
      subscription_eligibility::SubscriptionEligibilityService,
      subscription_eligibility::SubscriptionEligibilityService::Observer>
      subscription_eligibility_observer_{this};
  // Cached last enablement state.
  RemoteAnnotatorEnablementState enablement_state_ =
      RemoteAnnotatorEnablementState::kDisabledNotEligible;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service.h"
#include "components/accessibility_annotator/core/country_type.h"

namespace account_settings {
class AccountSettingService;
}  // namespace account_settings

namespace signin {
class IdentityManager;
}  // namespace signin

namespace accessibility_annotator {

class AccessibilityAnnotatorEnablementServiceImpl
    : public AccessibilityAnnotatorEnablementService {
 public:
  explicit AccessibilityAnnotatorEnablementServiceImpl(
      account_settings::AccountSettingService* account_settings_service,
      signin::IdentityManager* identity_manager,
      GeoIpCountryCode country_code);
  AccessibilityAnnotatorEnablementServiceImpl(
      const AccessibilityAnnotatorEnablementServiceImpl&) = delete;
  AccessibilityAnnotatorEnablementServiceImpl& operator=(
      const AccessibilityAnnotatorEnablementServiceImpl&) = delete;
  ~AccessibilityAnnotatorEnablementServiceImpl() override;

  // AccessibilityAnnotatorEnablementService:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  RemoteAnnotatorEnablementState GetEnablementState() override;

 private:
  const raw_ptr<account_settings::AccountSettingService>
      account_settings_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const GeoIpCountryCode country_code_;
  base::ObserverList<Observer> observers_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_H_

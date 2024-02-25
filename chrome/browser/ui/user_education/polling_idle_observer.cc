// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/polling_idle_observer.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/user_education/common/feature_promo_idle_observer.h"
#include "components/user_education/common/feature_promo_idle_policy.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/user_education_features.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/idle_polling_service.h"

// Function called by `UserEducationServiceFactory` to create an idle observer.
std::unique_ptr<user_education::FeaturePromoIdleObserver>
CreatePollingIdleObserver() {
  user_education::FeaturePromoIdlePolicy::SetIdleTimeResolution(
      ui::IdlePollingService::kPollInterval);
  return std::make_unique<PollingIdleObserver>();
}

PollingIdleObserver::PollingIdleObserver() = default;
PollingIdleObserver::~PollingIdleObserver() = default;

PollingIdleObserver::IdleState PollingIdleObserver::GetCurrentState() const {
  const base::Time last_known_active_time =
      GetCurrentTime() - base::Seconds(ui::CalculateIdleTime());
  const bool application_is_active =
      IsChromeActive() && !ui::CheckIdleStateIsLocked();
  return IdleState{last_known_active_time, application_is_active};
}

void PollingIdleObserver::StartObserving() {
  service_observer_.Observe(ui::IdlePollingService::GetInstance());
}

void PollingIdleObserver::OnIdleStateChange(
    const ui::IdlePollingService::State& state) {
  const base::Time active_time = GetCurrentTime() - state.idle_time;
  const bool application_active = !state.locked && IsChromeActive();
  NotifyIdleStateChanged(IdleState{active_time, application_active});
}

bool PollingIdleObserver::IsChromeActive() const {
  const auto* const browser = BrowserList::GetInstance()->GetLastActive();
  return browser && browser->window()->IsActive();
}

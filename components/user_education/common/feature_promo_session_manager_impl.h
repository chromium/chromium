// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_IMPL_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_IMPL_H_

#include "base/scoped_observation.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "ui/base/idle/idle_polling_service.h"

namespace user_education {

// Used to observe the system/application idle state. Override virtual methods
// for testing.
class PollingIdleObserver : public FeaturePromoSessionManager::IdleObserver,
                            public ui::IdlePollingService::Observer {
 public:
  PollingIdleObserver();
  ~PollingIdleObserver() override;

  // IdleObserver:
  void StartObserving() override;

  // Returns the current idle state. Used on startup and shutdown.
  // Override for testing.
  FeaturePromoSessionManager::IdleState GetCurrentState() const override;

 private:
  // ui::IdlePollingService::Observer:
  void OnIdleStateChange(const ui::IdlePollingService::State& state) final;

  base::ScopedObservation<ui::IdlePollingService,
                          ui::IdlePollingService::Observer>
      service_observer_{this};
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SESSION_MANAGER_IMPL_H_

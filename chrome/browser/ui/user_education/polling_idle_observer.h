// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_POLLING_IDLE_OBSERVER_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_POLLING_IDLE_OBSERVER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "ui/base/idle/idle_polling_service.h"

// Used to observe the system/application idle state. Override virtual methods
// for testing.
class PollingIdleObserver :
    public user_education::FeaturePromoSessionManager::IdleObserver,
    public ui::IdlePollingService::Observer {
 public:
  using IdleState = user_education::FeaturePromoSessionManager::IdleState;

  PollingIdleObserver();
  ~PollingIdleObserver() override;

  // IdleObserver:
  void StartObserving() override;

  // Returns the current idle state. Used on startup and shutdown.
  // Override for testing.
  IdleState GetCurrentState() const override;

 private:
  // Returns whether the current application is active.
  bool IsChromeActive() const;

  // ui::IdlePollingService::Observer:
  void OnIdleStateChange(const ui::IdlePollingService::State& state) final;

  base::ScopedObservation<ui::IdlePollingService,
                          ui::IdlePollingService::Observer>
      service_observer_{this};
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_POLLING_IDLE_OBSERVER_H_

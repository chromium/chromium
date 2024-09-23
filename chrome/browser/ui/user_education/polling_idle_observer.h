// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_POLLING_IDLE_OBSERVER_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_POLLING_IDLE_OBSERVER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "ui/base/idle/idle_polling_service.h"

// Used to observe the system/application idle state, for purposes of session
// tracking for User Education. This implementation uses system calls to observe
// the locked and idle state as well as the presence of a foregrounded browser
// window.
class PollingIdleObserver : public user_education::FeaturePromoIdleObserver,
                            public ui::IdlePollingService::Observer {
 public:
  PollingIdleObserver();
  ~PollingIdleObserver() override;

  // IdleObserver:
  void StartObserving() override;
  std::optional<base::Time> MaybeGetNewLastActiveTime() const override;

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

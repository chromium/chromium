// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/polling_idle_observer.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/user_education/common/feature_promo_idle_observer.h"
#include "components/user_education/common/feature_promo_idle_policy.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/user_education_features.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/idle_polling_service.h"

namespace {
// Last active time is "stale" if it is significantly before the last time the
// idle state would have been polled. A state last active time means the
// browser is not actually active. There is a "fudge factor" of 50% or 2
// seconds (whichever is larger) to account for the low resolution of
// `ui::CalculateIdleTime()`.
static constexpr base::TimeDelta kStaleDataThreshold =
    std::max(ui::IdlePollingService::kPollInterval * 1.5,
             ui::IdlePollingService::kPollInterval + base::Seconds(2));
}  // namespace

// Function called by `UserEducationServiceFactory` to create an idle observer.
std::unique_ptr<user_education::FeaturePromoIdleObserver>
CreatePollingIdleObserver() {
  return std::make_unique<PollingIdleObserver>();
}

PollingIdleObserver::PollingIdleObserver() = default;
PollingIdleObserver::~PollingIdleObserver() = default;

std::optional<base::Time> PollingIdleObserver::MaybeGetNewLastActiveTime()
    const {
  // Determine how long ago the system was last active.
  const auto idle_time = base::Seconds(ui::CalculateIdleTime());

  // The browser is active if:
  //  - The last active time is not stale; if the last active time is too long
  //    ago, then the application is not active.
  //  - There is at least one active browser window; minimized browser windows
  //    or windows open but in the background do not count as using the browser.
  //  - The computer isn't locked; theoretically no browsers should be active in
  //    this situation, but it's unclear how each platform handles this. Also,
  //    since this is a system call, do it last, since it could be somewhat more
  //    expensive.
  if (idle_time > kStaleDataThreshold || !IsChromeActive() ||
      ui::CheckIdleStateIsLocked()) {
    return std::nullopt;
  }

  // Calling `GetCurrentTime()` here is slightly wrong, since some time has
  // elapsed since `idle_time` was retrieved, but the value only has one second
  // granularity, and it's only used for session computation, so it's not wrong
  // enough to matter.
  return GetCurrentTime() - idle_time;
}

void PollingIdleObserver::StartObserving() {
  service_observer_.Observe(ui::IdlePollingService::GetInstance());
}

void PollingIdleObserver::OnIdleStateChange(
    const ui::IdlePollingService::State& state) {
  if (state.locked || state.idle_time > kStaleDataThreshold ||
      !IsChromeActive()) {
    return;
  }
  const base::Time active_time = GetCurrentTime() - state.idle_time;
  NotifyLastActiveChanged(active_time);
}

bool PollingIdleObserver::IsChromeActive() const {
  const auto* const browser = BrowserList::GetInstance()->GetLastActive();
  return browser && browser->window()->IsActive();
}

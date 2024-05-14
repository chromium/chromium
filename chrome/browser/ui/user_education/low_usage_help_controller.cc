// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/low_usage_help_controller.h"

#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace {
const void* const kLowUsageHelpControllerKey = &kLowUsageHelpControllerKey;
constexpr base::TimeDelta kRetryDelay = base::Seconds(1);
}

LowUsageHelpController::LowUsageHelpController(Profile* profile)
    : profile_(profile) {
  if (UserEducationService* const service =
          UserEducationServiceFactory::GetForBrowserContext(profile)) {
    if (RecentSessionObserver* const observer =
            service->recent_session_observer()) {
      subscription_ = observer->AddLowUsageSessionCallback(
          base::BindRepeating(&LowUsageHelpController::OnLowUsageSession,
                              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

LowUsageHelpController::~LowUsageHelpController() = default;

// static
LowUsageHelpController* LowUsageHelpController::MaybeCreateForProfile(
    Profile* profile) {
  if (auto* const data = profile->GetUserData(kLowUsageHelpControllerKey)) {
    return static_cast<LowUsageHelpController*>(data);
  }
  auto new_data_ptr = base::WrapUnique(new LowUsageHelpController(profile));
  auto* const new_data = new_data_ptr.get();
  profile->SetUserData(kLowUsageHelpControllerKey, std::move(new_data_ptr));
  return new_data;
}

LowUsageHelpController* LowUsageHelpController::GetForProfileForTesting(
    Profile* profile) {
  if (auto* const data = profile->GetUserData(kLowUsageHelpControllerKey)) {
    return static_cast<LowUsageHelpController*>(data);
  }
  return nullptr;
}

void LowUsageHelpController::OnLowUsageSession() {
  retrying_ = false;
  retry_timer_.Stop();

  // Always want to try to show a promo on a fresh call stack.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LowUsageHelpController::MaybeShowPromo,
                                weak_ptr_factory_.GetWeakPtr()));
}

void LowUsageHelpController::MaybeShowPromo() {
  // Get the most recent active browser in the profile.
  auto* const browser = chrome::FindBrowserWithProfile(profile_);
  if (!browser) {
    // This can happen if windows are still loading up; that's fine.

    // Record the try count.
    const int try_count = retrying_ ? 2 : 1;
    UMA_HISTOGRAM_EXACT_LINEAR(
        "UserEducation.MessageNotShown.DesktopReEngagement.NoBrowser",
        try_count, 3);

    // Try again with a small delay. If this was already a retry, then just
    // don't show the promo.
    if (!retrying_) {
      retrying_ = true;
      retry_timer_.Start(FROM_HERE, kRetryDelay,
                         base::BindOnce(&LowUsageHelpController::MaybeShowPromo,
                                        weak_ptr_factory_.GetWeakPtr()));
    }

    // Either way, don't try to show the promo right now without a window.
    return;
  }

  const bool result = browser->window()->MaybeShowStartupFeaturePromo(
      feature_engagement::kIPHDesktopReEngagementFeature);

  (void)result;
}

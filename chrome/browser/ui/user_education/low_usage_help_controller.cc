// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/low_usage_help_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace {
const void* const kLowUsageHelpControllerKey = &kLowUsageHelpControllerKey;
}

LowUsageHelpController::LowUsageHelpController(Browser* browser)
    : browser_(browser) {
  if (UserEducationService* const service =
          UserEducationServiceFactory::GetForBrowserContext(
              browser->profile())) {
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
LowUsageHelpController* LowUsageHelpController::MaybeCreateForBrowser(
    Browser* browser) {
  if (auto* const data = browser->GetUserData(kLowUsageHelpControllerKey)) {
    return static_cast<LowUsageHelpController*>(data);
  }
  auto new_data_ptr = base::WrapUnique(new LowUsageHelpController(browser));
  auto* const new_data = new_data_ptr.get();
  browser->SetUserData(kLowUsageHelpControllerKey, std::move(new_data_ptr));
  return new_data;
}

LowUsageHelpController* LowUsageHelpController::GetForBrowserForTesting(
    Browser* browser) {
  if (auto* const data = browser->GetUserData(kLowUsageHelpControllerKey)) {
    return static_cast<LowUsageHelpController*>(data);
  }
  return nullptr;
}

void LowUsageHelpController::OnLowUsageSession() {
  // Always want to try to show a promo on a fresh call stack.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LowUsageHelpController::MaybeShowPromo,
                                weak_ptr_factory_.GetWeakPtr()));
}

void LowUsageHelpController::MaybeShowPromo() {
  BrowserWindow* const window = browser_->window();

  // Only ever want to show a promo in the active window.
  if ((user_education::FeaturePromoControllerCommon::
           active_window_check_blocked() ||
       window->IsActive()) &&
      window->MaybeShowStartupFeaturePromo(
          feature_engagement::kIPHDesktopReEngagementFeature)) {
    // TODO(dfried): maybe write some additional telemetry here (though just
    // checking the show result histograms should be fairly informative).
  }
}

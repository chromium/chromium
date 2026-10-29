// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_browser_controller.h"

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_button_controller.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"
#endif

namespace contextual_tasks {

DEFINE_USER_DATA(ContextualTasksBrowserController);

ContextualTasksBrowserController::ContextualTasksBrowserController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {
  active_task_context_provider_ =
      std::make_unique<ActiveTaskContextProviderImpl>(
          browser_window_interface_,
          ContextualTasksServiceFactory::GetForProfile(
              browser_window_interface_->GetProfile()));

  eligibility_manager_ =
      std::make_unique<EntryPointEligibilityManager>(browser_window_interface_);

  side_panel_coordinator_ =
      std::make_unique<ContextualTasksSidePanelCoordinator>(
          browser_window_interface_,
          ContextualTasksPanelHost::Create(browser_window_interface_),
          active_task_context_provider_.get(), eligibility_manager_.get());

#if !BUILDFLAG(IS_ANDROID)
  if (kShowEntryPoint.Get() == EntryPointOption::kToolbarEphemeralBranded ||
      IsContextualTasksPinnedButtonActiveIndicatorEnabled()) {
    ephemeral_button_controller_ =
        std::make_unique<ContextualTasksEphemeralButtonController>(
            browser_window_interface_);
  }

  close_button_controller_ =
      std::make_unique<ContextualTasksCloseButtonController>(
          browser_window_interface_, eligibility_manager_.get(),
          side_panel_coordinator_.get());

  Profile* const profile = browser_window_interface_->GetProfile();
  was_pinned_ = GetEffectivePinState(profile);
  if (auto* pinned_model = PinnedToolbarActionsModel::Get(profile)) {
    pinned_toolbar_observation_.Observe(pinned_model);
  }

  UpdatePinButtonVisibilityState(browser_window_interface_);
  MaybeShowPinnedIconChangedPromo();

  // This subscription is safe because the browser_window_interface_
  // BrowserWindowFeatures, which owns this controller and this subscription.
  actions_visibility_subscription_ =
      eligibility_manager_->RegisterOnEntryPointEligibilityChanged(
          base::BindRepeating(
              &ContextualTasksBrowserController::OnEntryPointEligibilityChanged,
              base::Unretained(this)));
#endif
}

void ContextualTasksBrowserController::Shutdown() {
#if !BUILDFLAG(IS_ANDROID)
  actions_visibility_subscription_ = base::CallbackListSubscription();
  pinned_toolbar_observation_.Reset();

  close_button_controller_.reset();
  ephemeral_button_controller_.reset();
#endif
  if (side_panel_coordinator_) {
    // Tell the side panel coordinator to disconnect its observers safely.
    side_panel_coordinator_.reset();
  }
}

ContextualTasksBrowserController::~ContextualTasksBrowserController() = default;

// static
ContextualTasksBrowserController* ContextualTasksBrowserController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

#if !BUILDFLAG(IS_ANDROID)
void ContextualTasksBrowserController::OnActionsChanged() {
  if (!browser_window_interface_) {
    return;
  }
  Profile* const profile = browser_window_interface_->GetProfile();
  if (!profile || profile->IsOffTheRecord()) {
    return;
  }
  const bool is_pinned = GetEffectivePinState(profile);
  if (is_pinned == was_pinned_) {
    return;
  }
  was_pinned_ = is_pinned;
  if (is_pinned) {
    // If the user pins the button while already in a Super G icon arm, mark
    // the "icon changed" promo as dismissed and used so it will not trigger on
    // future startups or new windows.
    if (ShouldUseSuperGIconForPinButtons() ||
        ShouldUseGraySuperGIconForPinButtons()) {
      if (auto* const service =
              UserEducationServiceFactory::GetForBrowserContext(profile)) {
        auto& storage = service->user_education_storage_service();
        user_education::FeaturePromoData data =
            storage
                .ReadPromoData(feature_engagement::
                                   kIPHContextualTasksPinnedIconChangedFeature)
                .value_or(user_education::FeaturePromoData());
        data.is_dismissed = true;
        data.last_dismissed_by =
            user_education::FeaturePromoClosedReason::kFeatureEngaged;
        storage.SavePromoData(
            feature_engagement::kIPHContextualTasksPinnedIconChangedFeature,
            data);
      }
      if (auto* const user_ed =
              BrowserUserEducationInterface::From(browser_window_interface_)) {
        user_ed->NotifyFeaturePromoFeatureUsed(
            feature_engagement::kIPHContextualTasksPinnedIconChangedFeature,
            FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
      }
    }
  } else if (auto* const user_ed =
                 BrowserUserEducationInterface::From(browser_window_interface_)) {
    user_ed->AbortFeaturePromo(
        feature_engagement::kIPHContextualTasksPinnedIconChangedFeature);
  }
}

void ContextualTasksBrowserController::OnEntryPointEligibilityChanged(
    bool /*is_eligible*/) {
  UpdatePinButtonVisibilityState(browser_window_interface_);
  MaybeShowPinnedIconChangedPromo();
}

void ContextualTasksBrowserController::MaybeShowPinnedIconChangedPromo() {
  if (has_requested_pinned_icon_changed_promo_ || !browser_window_interface_) {
    return;
  }
  Profile* const profile = browser_window_interface_->GetProfile();
  if (!profile || profile->IsOffTheRecord()) {
    return;
  }
  // Show the promo at startup if the user is in a Super G arm and already has
  // the toolbar button pinned. Users who pinned or clicked the button after
  // entering the Super G arm have already dismissed the promo or notified
  // Feature Engagement that the feature was used, which suppresses the promo.
  const bool is_using_super_g_icon =
      ShouldUseSuperGIconForPinButtons() ||
      ShouldUseGraySuperGIconForPinButtons();
  if (!is_using_super_g_icon) {
    return;
  }
  if (!GetEffectivePinState(profile) ||
      !EntryPointEligibilityManager::IsPinningEligible(profile)) {
    return;
  }
  if (auto* const user_ed =
          BrowserUserEducationInterface::From(browser_window_interface_)) {
    has_requested_pinned_icon_changed_promo_ = true;
    if (!user_ed->HasFeaturePromoBeenDismissed(
            feature_engagement::kIPHContextualTasksPinnedIconChangedFeature)) {
      user_ed->MaybeShowStartupFeaturePromo(
          feature_engagement::kIPHContextualTasksPinnedIconChangedFeature);
    }
  }
}
#endif

}  // namespace contextual_tasks

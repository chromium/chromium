// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"

#include <cstdint>

#include "base/byte_count.h"
#include "base/check_is_test.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_controller.h"
#include "chrome/browser/ui/performance_controls/memory_saver_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/visibility.h"
#include "content/public/common/url_constants.h"

namespace {
using ::performance_manager::user_tuning::UserPerformanceTuningManager;
}  // namespace

MemorySaverChipTabHelper::~MemorySaverChipTabHelper() = default;

void MemorySaverChipTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Pages can only be discarded while they are in the background, and we only
  // need to inform the user after they have been subsequently reloaded so it
  // is sufficient to wait for a StartNavigation event before updating this
  // variable.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    // Ignore navigations from inner frames because we only care about
    // top-level discards. Ignore same-document navigations because actual
    // discard reloads will not be same-document navigations and including
    // them causes the state to get reset.
    return;
  }

  was_rendered_ = false;

  ComputeChipState(navigation_handle);
}

void MemorySaverChipTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    was_rendered_ = false;
    if (chip_state_ == memory_saver::ChipState::EXPANDED_WITH_SAVINGS ||
        chip_state_ == memory_saver::ChipState::EXPANDED_EDUCATION) {
      chip_state_ = memory_saver::ChipState::COLLAPSED_FROM_EXPANDED;
      UpdatePageActionState();
    }
  }
}

bool MemorySaverChipTabHelper::ShouldChipAnimate() {
  bool should_animate = !was_rendered_;
  was_rendered_ = true;
  return should_animate;
}

MemorySaverChipTabHelper::MemorySaverChipTabHelper(tabs::TabInterface& tab)
    : ContentsObservingTabFeature(tab) {
  pref_service_ =
      Profile::FromBrowserContext(tab.GetBrowserWindowInterface()->GetProfile())
          ->GetPrefs();

  if (UserPerformanceTuningManager::HasInstance()) {
    user_performance_tuning_manager_observation_.Observe(
        UserPerformanceTuningManager::GetInstance());
    is_memory_saver_mode_enabled_ =
        UserPerformanceTuningManager::GetInstance()->IsMemorySaverModeActive();
  } else {
    // Some unit tests don't have a UserPerformanceTuningManager.
    CHECK_IS_TEST();
  }
}

bool MemorySaverChipTabHelper::ComputeShouldHighlightMemorySavings() {
  bool const savings_over_threshold =
      memory_saver::GetDiscardedMemorySavings(web_contents()) >
      kExpandedMemorySaverChipThreshold;

  base::Time const last_expanded_timestamp =
      pref_service_->GetTime(prefs::kLastMemorySaverChipExpandedTimestamp);
  bool const expanded_chip_not_shown_recently =
      (base::Time::Now() - last_expanded_timestamp) >
      kExpandedMemorySaverChipFrequency;

  auto* const pre_discard_resource_usage =
      UserPerformanceTuningManager::PreDiscardResourceUsage::FromWebContents(
          web_contents());
  bool const tab_discard_time_over_threshold =
      pre_discard_resource_usage &&
      (base::LiveTicks::Now() -
       pre_discard_resource_usage->discard_live_ticks()) >
          kExpandedMemorySaverChipDiscardedDuration;

  if (savings_over_threshold && expanded_chip_not_shown_recently &&
      tab_discard_time_over_threshold) {
    pref_service_->SetTime(prefs::kLastMemorySaverChipExpandedTimestamp,
                           base::Time::Now());
    return true;
  }
  return false;
}

bool MemorySaverChipTabHelper::ComputeShouldEducateAboutMemorySavings() {
  int times_rendered =
      pref_service_->GetInteger(prefs::kMemorySaverChipExpandedCount);
  if (times_rendered < kChipAnimationCount) {
    pref_service_->SetInteger(prefs::kMemorySaverChipExpandedCount,
                              times_rendered + 1);
    return true;
  }
  return false;
}

void MemorySaverChipTabHelper::ComputeChipState(
    content::NavigationHandle* navigation_handle) {
  // This memory saver chip only appears for eligible sites that have been
  // proactively discarded.
  bool const was_discarded = navigation_handle->ExistingDocumentWasDiscarded();
  std::optional<mojom::LifecycleUnitDiscardReason> const discard_reason =
      memory_saver::GetDiscardReason(navigation_handle->GetWebContents());
  bool const is_site_supported =
      memory_saver::IsURLSupported(navigation_handle->GetURL());

  if (!(is_memory_saver_mode_enabled_ && was_discarded &&
        (discard_reason == mojom::LifecycleUnitDiscardReason::PROACTIVE ||
         discard_reason == mojom::LifecycleUnitDiscardReason::SUGGESTED) &&
        is_site_supported)) {
    chip_state_ = memory_saver::ChipState::HIDDEN;
  } else if (ComputeShouldEducateAboutMemorySavings()) {
    chip_state_ = memory_saver::ChipState::EXPANDED_EDUCATION;
  } else if (ComputeShouldHighlightMemorySavings()) {
    chip_state_ = memory_saver::ChipState::EXPANDED_WITH_SAVINGS;
  } else {
    chip_state_ = memory_saver::ChipState::COLLAPSED;
  }

  UpdatePageActionState();
}

void MemorySaverChipTabHelper::UpdatePageActionState() {
  if (!IsPageActionMigrated(PageActionIconType::kMemorySaver)) {
    return;
  }

  tabs::TabFeatures* tab_features = tab().GetTabFeatures();
  if (!tab_features) {
    // Tab features may not be present at shutdown.
    return;
  }
  memory_saver::MemorySaverChipController* controller =
      tab_features->memory_saver_chip_controller();
  switch (chip_state_) {
    case memory_saver::ChipState::HIDDEN:
      controller->Hide();
      break;
    case memory_saver::ChipState::COLLAPSED:
    case memory_saver::ChipState::COLLAPSED_FROM_EXPANDED:
      controller->ShowIcon();
      break;
    case memory_saver::ChipState::EXPANDED_EDUCATION:
      controller->ShowEducationChip();
      break;

    case memory_saver::ChipState::EXPANDED_WITH_SAVINGS:
      const base::ByteCount bytes_saved =
          memory_saver::GetDiscardedMemorySavings(web_contents());
      controller->ShowMemorySavedChip(bytes_saved);
      break;
  }
}

void MemorySaverChipTabHelper::OnMemorySaverModeChanged() {
  is_memory_saver_mode_enabled_ =
      UserPerformanceTuningManager::GetInstance()->IsMemorySaverModeActive();

  // If disabling the feature, clear any active UI. If enabling, let future
  // navigation events show the UI.
  if (!is_memory_saver_mode_enabled_) {
    chip_state_ = memory_saver::ChipState::HIDDEN;
    UpdatePageActionState();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MemorySaverChipTabHelper);

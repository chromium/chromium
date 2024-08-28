// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/performance_controls/memory_saver_utils.h"
#include "chrome/common/pref_names.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "content/public/browser/visibility.h"
#include "content/public/common/url_constants.h"

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
    }
  }
}

bool MemorySaverChipTabHelper::ShouldChipAnimate() {
  bool should_animate = !was_rendered_;
  was_rendered_ = true;
  return should_animate;
}

MemorySaverChipTabHelper::MemorySaverChipTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<MemorySaverChipTabHelper>(*contents) {
  pref_service_ =
      Profile::FromBrowserContext(contents->GetBrowserContext())->GetPrefs();
}

bool MemorySaverChipTabHelper::ComputeShouldHighlightMemorySavings() {
  bool const savings_over_threshold =
      memory_saver::GetDiscardedMemorySavingsInBytes(&GetWebContents()) >
      kExpandedMemorySaverChipThresholdBytes;

  base::Time const last_expanded_timestamp =
      pref_service_->GetTime(prefs::kLastMemorySaverChipExpandedTimestamp);
  bool const expanded_chip_not_shown_recently =
      (base::Time::Now() - last_expanded_timestamp) >
      kExpandedMemorySaverChipFrequency;

  auto* const pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::FromWebContents(&GetWebContents());
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

  if (!(was_discarded &&
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
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MemorySaverChipTabHelper);

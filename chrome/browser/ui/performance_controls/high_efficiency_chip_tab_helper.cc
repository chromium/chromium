// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/high_efficiency_chip_tab_helper.h"

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_utils.h"
#include "chrome/common/pref_names.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "content/public/browser/visibility.h"
#include "content/public/common/url_constants.h"

HighEfficiencyChipTabHelper::~HighEfficiencyChipTabHelper() = default;

void HighEfficiencyChipTabHelper::DidStartNavigation(
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

void HighEfficiencyChipTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    was_rendered_ = false;
    if (chip_state_ == high_efficiency::ChipState::EXPANDED_WITH_SAVINGS ||
        chip_state_ == high_efficiency::ChipState::EXPANDED_EDUCATION) {
      chip_state_ = high_efficiency::ChipState::COLLAPSED_FROM_EXPANDED;
    }
  }
}

bool HighEfficiencyChipTabHelper::ShouldChipAnimate() {
  bool should_animate = !was_rendered_;
  was_rendered_ = true;
  return should_animate;
}

HighEfficiencyChipTabHelper::HighEfficiencyChipTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<HighEfficiencyChipTabHelper>(*contents) {
  pref_service_ =
      Profile::FromBrowserContext(contents->GetBrowserContext())->GetPrefs();
}

bool HighEfficiencyChipTabHelper::ComputeShouldHighlightMemorySavings() {
  if (!base::FeatureList::IsEnabled(
          performance_manager::features::kMemorySavingsReportingImprovements)) {
    return false;
  }

  bool const savings_over_threshold =
      static_cast<int>(high_efficiency::GetDiscardedMemorySavingsInBytes(
          &GetWebContents())) >
      performance_manager::features::kExpandedHighEfficiencyChipThresholdBytes
          .Get();

  base::Time const last_expanded_timestamp =
      pref_service_->GetTime(prefs::kLastHighEfficiencyChipExpandedTimestamp);
  bool const expanded_chip_not_shown_recently =
      (base::Time::Now() - last_expanded_timestamp) >
      performance_manager::features::kExpandedHighEfficiencyChipFrequency.Get();

  auto* pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::FromWebContents(&GetWebContents());
  bool const tab_discard_time_over_threshold =
      pre_discard_resource_usage &&
      (base::LiveTicks::Now() -
       pre_discard_resource_usage->discard_liveticks()) >
          performance_manager::features::
              kExpandedHighEfficiencyChipDiscardedDuration.Get();

  if (savings_over_threshold && expanded_chip_not_shown_recently &&
      tab_discard_time_over_threshold) {
    pref_service_->SetTime(prefs::kLastHighEfficiencyChipExpandedTimestamp,
                           base::Time::Now());
    return true;
  }
  return false;
}

bool HighEfficiencyChipTabHelper::ComputeShouldEducateAboutMemorySavings() {
  int times_rendered =
      pref_service_->GetInteger(prefs::kHighEfficiencyChipExpandedCount);
  if (times_rendered < kChipAnimationCount) {
    pref_service_->SetInteger(prefs::kHighEfficiencyChipExpandedCount,
                              times_rendered + 1);
    return true;
  }
  return false;
}

void HighEfficiencyChipTabHelper::ComputeChipState(
    content::NavigationHandle* navigation_handle) {
  // This high efficiency chip only appears for eligible sites that have been
  // proactively discarded.
  bool const was_discarded = navigation_handle->ExistingDocumentWasDiscarded();
  absl::optional<mojom::LifecycleUnitDiscardReason> const discard_reason =
      high_efficiency::GetDiscardReason(navigation_handle->GetWebContents());
  bool const is_site_supported =
      high_efficiency::IsURLSupported(navigation_handle->GetURL());

  if (!(was_discarded &&
        discard_reason == mojom::LifecycleUnitDiscardReason::PROACTIVE &&
        is_site_supported)) {
    chip_state_ = high_efficiency::ChipState::HIDDEN;
  } else if (ComputeShouldEducateAboutMemorySavings()) {
    chip_state_ = high_efficiency::ChipState::EXPANDED_EDUCATION;
  } else if (ComputeShouldHighlightMemorySavings()) {
    chip_state_ = high_efficiency::ChipState::EXPANDED_WITH_SAVINGS;
  } else {
    chip_state_ = high_efficiency::ChipState::COLLAPSED;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HighEfficiencyChipTabHelper);

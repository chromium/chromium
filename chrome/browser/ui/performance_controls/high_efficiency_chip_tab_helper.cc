// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/high_efficiency_chip_tab_helper.h"

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_utils.h"
#include "content/public/common/url_constants.h"

namespace {
// Conversion constant for bytes to kilobytes.
constexpr size_t kKiloByte = 1024;
}  // namespace

HighEfficiencyChipTabHelper::~HighEfficiencyChipTabHelper() = default;

HighEfficiencyChipTabHelper::HighEfficiencyChipTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<HighEfficiencyChipTabHelper>(*contents) {}

bool HighEfficiencyChipTabHelper::ShouldChipBeVisible() const {
  return was_discarded_ && is_site_supported_ && IsProactiveDiscard();
}

bool HighEfficiencyChipTabHelper::ShouldIconAnimate() const {
  return was_discarded_ && !was_animated_ && IsProactiveDiscard();
}

void HighEfficiencyChipTabHelper::SetWasAnimated() {
  was_animated_ = true;
}

void HighEfficiencyChipTabHelper::SetChipHasBeenHidden() {
  was_chip_hidden_ = true;
}

bool HighEfficiencyChipTabHelper::HasChipBeenHidden() {
  return was_chip_hidden_;
}

uint64_t HighEfficiencyChipTabHelper::GetMemorySavingsInBytes() const {
  auto* pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::FromWebContents(&GetWebContents());
  return pre_discard_resource_usage == nullptr
             ? 0
             : pre_discard_resource_usage->memory_footprint_estimate_kb() *
                   kKiloByte;
}

void HighEfficiencyChipTabHelper::SetSiteWasAddedToExclusionList() {
  was_site_added_to_exclusion_list_ = true;
}

bool HighEfficiencyChipTabHelper::GetWasSiteAddedToExclusionList() const {
  return was_site_added_to_exclusion_list_;
}

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
  discard_reason_ =
      high_efficiency::GetDiscardReason(navigation_handle->GetWebContents());
  was_discarded_ = navigation_handle->ExistingDocumentWasDiscarded();
  was_animated_ = false;
  was_chip_hidden_ = false;
  was_site_added_to_exclusion_list_ = false;
  is_site_supported_ =
      high_efficiency::IsURLSupported(navigation_handle->GetURL());
}

bool HighEfficiencyChipTabHelper::IsProactiveDiscard() const {
  return discard_reason_.has_value() &&
         discard_reason_.value() ==
             mojom::LifecycleUnitDiscardReason::PROACTIVE;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HighEfficiencyChipTabHelper);

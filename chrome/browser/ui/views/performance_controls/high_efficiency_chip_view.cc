// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"
#include <string>

#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_bubble_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/views/view_class_properties.h"

namespace {

// The duration that the chip should be expanded for.
constexpr base::TimeDelta kChipAnimationDuration = base::Seconds(12);

}  // namespace

HighEfficiencyChipView::HighEfficiencyChipView(
    CommandUpdater* command_updater,
    Browser* browser,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "HighEfficiency"),
      browser_(browser),
      chip_accessible_label_(
          l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_CHIP_ACCNAME)) {
  DCHECK(browser_);

  auto* manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  user_performance_tuning_manager_observation_.Observe(manager);
  OnHighEfficiencyModeChanged();

  SetUpForInOutAnimation(kChipAnimationDuration);
  SetPaintLabelOverSolidBackground(true);
  SetProperty(views::kElementIdentifierKey, kHighEfficiencyChipElementId);
  browser_->tab_strip_model()->AddObserver(this);
}

HighEfficiencyChipView::~HighEfficiencyChipView() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

void HighEfficiencyChipView::OnBubbleShown() {
  PauseAnimation();
}

void HighEfficiencyChipView::OnBubbleHidden() {
  UnpauseAnimation();
  bubble_ = nullptr;
}

void HighEfficiencyChipView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  content::WebContents* const web_contents = selection.old_contents;
  if (!web_contents) {
    return;
  }

  if (selection.active_tab_changed()) {
    HighEfficiencyChipTabHelper* const tab_helper =
        HighEfficiencyChipTabHelper::FromWebContents(web_contents);
    tab_helper->SetChipHasBeenHidden();
  }
}

void HighEfficiencyChipView::UpdateImpl() {
  content::WebContents* const web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }
  HighEfficiencyChipTabHelper* const tab_helper =
      HighEfficiencyChipTabHelper::FromWebContents(web_contents);
  if (tab_helper->ShouldChipBeVisible() && is_high_efficiency_mode_enabled_) {
    SetVisible(true);
    if (tab_helper->ShouldIconAnimate()) {
      tab_helper->SetWasAnimated();
      // Show an informational message the first 3 times the chip is shown.
      PrefService* const pref_service = browser_->profile()->GetPrefs();
      int times_rendered =
          pref_service->GetInteger(prefs::kHighEfficiencyChipExpandedCount);
      if (times_rendered < kChipAnimationCount) {
        AnimateIn(IDS_HIGH_EFFICIENCY_CHIP_LABEL);
        pref_service->SetInteger(prefs::kHighEfficiencyChipExpandedCount,
                                 times_rendered + 1);
        RecordHighEfficiencyChipState(
            HighEfficiencyChipState::kExpandedEducation);
      } else if (ShouldHighlightMemorySavingsWithExpandedChip(tab_helper,
                                                              pref_service)) {
        int const memory_savings = tab_helper->GetMemorySavingsInBytes();
        std::u16string memory_savings_string = ui::FormatBytes(memory_savings);
        SetLabel(
            l10n_util::GetStringFUTF16(IDS_HIGH_EFFICIENCY_CHIP_SAVINGS_LABEL,
                                       {memory_savings_string}),
            l10n_util::GetStringFUTF16(
                IDS_HIGH_EFFICIENCY_CHIP_WITH_SAVINGS_ACCNAME,
                {memory_savings_string}));
        AnimateIn(absl::nullopt);
        pref_service->SetTime(prefs::kLastHighEfficiencyChipExpandedTimestamp,
                              base::Time::Now());
        RecordHighEfficiencyChipState(
            HighEfficiencyChipState::kExpandedWithSavings);
      } else {
        SetAccessibleName(chip_accessible_label_);
        RecordHighEfficiencyChipState(HighEfficiencyChipState::kCollapsed);
      }
    } else if (tab_helper->HasChipBeenHidden()) {
      UnpauseAnimation();
      AnimateOut();
      ResetSlideAnimation(false);
    }
  } else {
    AnimateOut();
    ResetSlideAnimation(false);
    SetVisible(false);
  }
}

void HighEfficiencyChipView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  // If the dialog bubble is currently open, close it.
  if (IsBubbleShowing()) {
    bubble_->Close();
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

  // Open the dialog bubble.
  View* anchor_view = browser_view->toolbar_button_provider()->GetAnchorView(
      PageActionIconType::kHighEfficiency);
  bubble_ = HighEfficiencyBubbleView::ShowBubble(browser_, anchor_view, this);
  if (browser_->window() != nullptr) {
    browser_->window()->NotifyFeatureEngagementEvent(
        feature_engagement::events::kHighEfficiencyDialogShown);
  }
}

const gfx::VectorIcon& HighEfficiencyChipView::GetVectorIcon() const {
  return OmniboxFieldTrial::IsChromeRefreshIconsEnabled()
             ? kHighEfficiencyChromeRefreshIcon
             : kHighEfficiencyIcon;
}

views::BubbleDialogDelegate* HighEfficiencyChipView::GetBubble() const {
  return bubble_;
}

void HighEfficiencyChipView::OnHighEfficiencyModeChanged() {
  auto* manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  is_high_efficiency_mode_enabled_ = manager->IsHighEfficiencyModeActive();
}

bool HighEfficiencyChipView::ShouldHighlightMemorySavingsWithExpandedChip(
    HighEfficiencyChipTabHelper* high_efficiency_tab_helper,
    PrefService* pref_service) {
  if (!base::FeatureList::IsEnabled(
          performance_manager::features::kMemorySavingsReportingImprovements)) {
    return false;
  }

  bool const savings_over_threshold =
      (int)high_efficiency_tab_helper->GetMemorySavingsInBytes() >
      performance_manager::features::kExpandedHighEfficiencyChipThresholdBytes
          .Get();

  base::Time const last_expanded_timestamp =
      pref_service->GetTime(prefs::kLastHighEfficiencyChipExpandedTimestamp);
  bool const expanded_chip_not_shown_recently =
      (base::Time::Now() - last_expanded_timestamp) >
      performance_manager::features::kExpandedHighEfficiencyChipFrequency.Get();

  auto* pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::FromWebContents(GetWebContents());
  bool const tab_discard_time_over_threshold =
      pre_discard_resource_usage &&
      (base::LiveTicks::Now() -
       pre_discard_resource_usage->discard_liveticks()) >
          performance_manager::features::
              kExpandedHighEfficiencyChipDiscardedDuration.Get();

  return savings_over_threshold && expanded_chip_not_shown_recently &&
         tab_discard_time_over_threshold;
}

BEGIN_METADATA(HighEfficiencyChipView, PageActionIconView)
END_METADATA

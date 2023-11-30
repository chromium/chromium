// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"
#include <string>

#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_utils.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
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
}

HighEfficiencyChipView::~HighEfficiencyChipView() = default;

void HighEfficiencyChipView::OnBubbleShown() {
  PauseAnimation();
}

void HighEfficiencyChipView::OnBubbleHidden() {
  UnpauseAnimation();
  bubble_ = nullptr;
}

void HighEfficiencyChipView::UpdateImpl() {
  content::WebContents* const web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  HighEfficiencyChipTabHelper* const tab_helper =
      HighEfficiencyChipTabHelper::FromWebContents(web_contents);
  auto chip_state = tab_helper->chip_state();

  if (chip_state != high_efficiency::ChipState::HIDDEN &&
      is_high_efficiency_mode_enabled_) {
    if (!tab_helper->ShouldChipAnimate()) {
      return;
    }

    switch (chip_state) {
      case high_efficiency::ChipState::EXPANDED_EDUCATION: {
        SetVisible(true);
        AnimateIn(IDS_HIGH_EFFICIENCY_CHIP_LABEL);
        RecordHighEfficiencyChipState(
            HighEfficiencyChipState::kExpandedEducation);
        break;
      }
      case high_efficiency::ChipState::EXPANDED_WITH_SAVINGS: {
        SetVisible(true);
        int const memory_savings =
            high_efficiency::GetDiscardedMemorySavingsInBytes(web_contents);
        std::u16string memory_savings_string = ui::FormatBytes(memory_savings);
        SetLabel(
            l10n_util::GetStringFUTF16(IDS_HIGH_EFFICIENCY_CHIP_SAVINGS_LABEL,
                                       {memory_savings_string}),
            l10n_util::GetStringFUTF16(
                IDS_HIGH_EFFICIENCY_CHIP_WITH_SAVINGS_ACCNAME,
                {memory_savings_string}));
        AnimateIn(absl::nullopt);
        RecordHighEfficiencyChipState(
            HighEfficiencyChipState::kExpandedWithSavings);
        break;
      }
      case high_efficiency::ChipState::COLLAPSED_FROM_EXPANDED: {
        SetVisible(true);
        UnpauseAnimation();
        AnimateOut();
        ResetSlideAnimation(false);
        break;
      }
      case high_efficiency::ChipState::COLLAPSED: {
        SetVisible(true);
        SetAccessibleName(chip_accessible_label_);
        RecordHighEfficiencyChipState(HighEfficiencyChipState::kCollapsed);
        break;
      }
      default: {
        NOTREACHED();
      }
    }
  } else {
    AnimateOut();
    ResetSlideAnimation(false);
    SetVisible(false);
  }
}

void HighEfficiencyChipView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  // If the performance side panel experiment is enabled, open the side panel.
  if (base::FeatureList::IsEnabled(
          performance_manager::features::kPerformanceControlsSidePanel)) {
    SidePanelUI::GetSidePanelUIForBrowser(browser_)->Show(
        SidePanelEntryId::kPerformance, SidePanelOpenTrigger::kToolbarButton);
    return;
  }

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

BEGIN_METADATA(HighEfficiencyChipView, PageActionIconView)
END_METADATA

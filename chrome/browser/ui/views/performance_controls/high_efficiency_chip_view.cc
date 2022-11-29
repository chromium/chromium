// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"

#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/performance_controls/tab_discard_tab_helper.h"
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
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace {

// The duration that the chip should be expanded for.
constexpr base::TimeDelta kChipAnimationDuration = base::Seconds(12);
// The delay before the IPH should be potentially shown. This should be less
// than kChipAnimationDuration but longer than kIconLabelAnimationDurationMs.
constexpr base::TimeDelta kIPHDelayDuration = base::Seconds(1);

// We want this to show up before the chip finishes animating.
static_assert(kIPHDelayDuration < kChipAnimationDuration);

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
      browser_(browser) {
  DCHECK(browser_);

  registrar_.Init(g_browser_process->local_state());
  registrar_.Add(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
      base::BindRepeating(&HighEfficiencyChipView::OnPrefChanged,
                          base::Unretained(this)));
  OnPrefChanged();

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
    TabDiscardTabHelper* const tab_helper =
        TabDiscardTabHelper::FromWebContents(web_contents);
    tab_helper->SetChipHasBeenHidden();
  }
}

void HighEfficiencyChipView::UpdateImpl() {
  content::WebContents* const web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }
  TabDiscardTabHelper* const tab_helper =
      TabDiscardTabHelper::FromWebContents(web_contents);
  if (tab_helper->IsChipVisible() && is_high_efficiency_mode_enabled_) {
    SetVisible(true);
    if (tab_helper->ShouldIconAnimate()) {
      // Only animate the chip to the expanded view the first 3 times it is
      // viewed.
      PrefService* const pref_service = browser_->profile()->GetPrefs();
      int times_rendered =
          pref_service->GetInteger(prefs::kHighEfficiencyChipExpandedCount);
      if (times_rendered < kChipAnimationCount) {
        AnimateIn(IDS_HIGH_EFFICIENCY_CHIP_LABEL);
        tab_helper->SetWasAnimated();
        pref_service->SetInteger(prefs::kHighEfficiencyChipExpandedCount,
                                 times_rendered + 1);
      }
    } else if (tab_helper->HasChipBeenHidden()) {
      ResetSlideAnimation(false);
    }

    if (performance_manager::features::kHighEfficiencyModeDefaultState.Get()) {
      // Delay the IPH to ensure the chip is not animating when it appears.
      timer_.Start(FROM_HERE, kIPHDelayDuration,
                   base::BindOnce(&HighEfficiencyChipView::MaybeShowIPH,
                                  weak_ptr_factory_.GetWeakPtr()));
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

  // If the IPH is currently open, close it before opening the dialog.
  browser_view->CloseFeaturePromo(
      feature_engagement::kIPHHighEfficiencyInfoModeFeature);

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
  return kHighEfficiencyIcon;
}

views::BubbleDialogDelegate* HighEfficiencyChipView::GetBubble() const {
  return bubble_;
}

std::u16string HighEfficiencyChipView::GetTextForTooltipAndAccessibleName()
    const {
  return l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_CHIP_ACCNAME);
}

void HighEfficiencyChipView::MaybeShowIPH() {
  if (browser_->window() != nullptr) {
    bool const promo_shown = browser_->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHHighEfficiencyInfoModeFeature, {},
        base::BindOnce(&HighEfficiencyChipView::OnIPHClosed,
                       weak_ptr_factory_.GetWeakPtr()));
    // While the IPH is showing, pause the animation of the chip so it doesn't
    // animate closed.
    if (promo_shown) {
      PauseAnimation();
      SetHighlighted(true);
    }
  }
}

void HighEfficiencyChipView::OnIPHClosed() {
  SetHighlighted(false);
  UnpauseAnimation();
}

void HighEfficiencyChipView::OnPrefChanged() {
  is_high_efficiency_mode_enabled_ = registrar_.prefs()->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);
}

BEGIN_METADATA(HighEfficiencyChipView, PageActionIconView)
END_METADATA

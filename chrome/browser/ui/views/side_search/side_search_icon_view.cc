// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_icon_view.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_metrics.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view_class_properties.h"

SideSearchIconView::SideSearchIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    Browser* browser)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate),
      browser_(browser),
      default_search_icon_source_(
          browser,
          base::BindRepeating(&SideSearchIconView::UpdateIconImage,
                              base::Unretained(this))) {
  image()->SetFlipCanvasOnPaintForRTLUI(false);
  SetProperty(views::kElementIdentifierKey, kSideSearchButtonElementId);
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_SIDE_SEARCH_ENTRYPOINT_LABEL));
  SetUpForInOutAnimation();
  SetPaintLabelOverSolidBackground(true);
  browser_->tab_strip_model()->AddObserver(this);
}

SideSearchIconView::~SideSearchIconView() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

void SideSearchIconView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed())
    HidePageActionLabel();
}

void SideSearchIconView::SetLabelVisibilityForTesting(bool visible) {
  label()->SetVisible(visible);
}

void SideSearchIconView::UpdateImpl() {
  content::WebContents* active_contents = GetWebContents();
  if (!active_contents)
    return;

  if (active_contents->IsCrashed()) {
    SetVisible(false);
    return;
  }

  // Only show the page action button if the side panel is showable for this
  // active web contents and is not currently toggled open.
  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(active_contents);
  if (!tab_contents_helper)
    return;

  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  const bool was_visible = GetVisible();
  const bool should_show =
      tab_contents_helper->CanShowSidePanelForCommittedNavigation() &&
      !tab_contents_helper->toggled_open();
  SetVisible(should_show);

  if (should_show && !was_visible) {
    if (ShouldShowPageActionLabel()) {
      SetPageActionLabelShown();
      should_extend_label_shown_duration_ = true;
      AnimateIn(absl::nullopt);
    } else if (tab_contents_helper->returned_to_previous_srp()) {
      // If we are not animating-in the label text make a request to show the
      // IPH if we detect the user may be engaging in a pogo-sticking journey.
      browser_view->MaybeShowFeaturePromo(
          feature_engagement::kIPHSideSearchFeature);
    }
  }

  if (!should_show) {
    HidePageActionLabel();
    browser_view->CloseFeaturePromo(feature_engagement::kIPHSideSearchFeature);
  }
}

void SideSearchIconView::OnExecuting(PageActionIconView::ExecuteSource source) {
  auto* side_search_browser_controller =
      BrowserView::GetBrowserViewForBrowser(browser_)->side_search_controller();
  RecordSideSearchPageActionLabelVisibilityOnToggle(
      label()->GetVisible() ? SideSearchPageActionLabelVisibility::kVisible
                            : SideSearchPageActionLabelVisibility::kNotVisible);

  // Reset the slide animation if in progress.
  HidePageActionLabel();

  side_search_browser_controller->ToggleSidePanel();

  auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
      browser_->profile());
  if (tracker)
    tracker->NotifyEvent(feature_engagement::events::kSideSearchOpened);
}

views::BubbleDialogDelegate* SideSearchIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& SideSearchIconView::GetVectorIcon() const {
  // Default to the kSearchIcon if the DSE icon image is not available.
  return vector_icons::kSearchIcon;
}

ui::ImageModel SideSearchIconView::GetSizedIconImage(int size) const {
  return default_search_icon_source_.GetSizedIconImage(size);
}

std::u16string SideSearchIconView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_TOOLTIP_SIDE_SEARCH_TOOLBAR_BUTTON_NOT_ACTIVATED);
}

void SideSearchIconView::AnimationProgressed(const gfx::Animation* animation) {
  PageActionIconView::AnimationProgressed(animation);
  // When the label is fully revealed pause the animation for
  // kLabelPersistDuration before resuming the animation and allowing the label
  // to animate out. This is currently set to show for 12s including the in/out
  // animation.
  // TODO(crbug.com/1314206): This approach of inspecting the animation progress
  // to extend the animation duration is quite hacky. This should be removed and
  // the IconLabelBubbleView API expanded to support a finer level of control.
  constexpr double kAnimationValueWhenLabelFullyShown = 0.5;
  constexpr base::TimeDelta kLabelPersistDuration = base::Milliseconds(10800);
  if (should_extend_label_shown_duration_ &&
      GetAnimationValue() >= kAnimationValueWhenLabelFullyShown) {
    should_extend_label_shown_duration_ = false;
    PauseAnimation();
    animate_out_timer_.Start(
        FROM_HERE, kLabelPersistDuration,
        base::BindOnce(&SideSearchIconView::UnpauseAnimation,
                       base::Unretained(this)));
  }
}

bool SideSearchIconView::ShouldShowPageActionLabel() const {
  content::WebContents* active_contents = GetWebContents();
  DCHECK(active_contents);

  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(active_contents);
  DCHECK(tab_contents_helper);

  if (!tab_contents_helper->GetAndResetCanShowPageActionLabel())
    return false;

  const int max_label_show_count =
      features::kSideSearchPageActionLabelAnimationMaxCount.Get();

  switch (features::kSideSearchPageActionLabelAnimationType.Get()) {
    case features::kSideSearchLabelAnimationTypeOption::kProfile: {
      auto* side_search_config =
          SideSearchConfig::Get(active_contents->GetBrowserContext());
      return side_search_config->page_action_label_shown_count() <
             max_label_show_count;
    }
    case features::kSideSearchLabelAnimationTypeOption::kWindow: {
      return page_action_label_shown_count_ < max_label_show_count;
    }
    case features::kSideSearchLabelAnimationTypeOption::kTab: {
      return tab_contents_helper->page_action_label_shown_count() <
             max_label_show_count;
    }
  }
}

void SideSearchIconView::SetPageActionLabelShown() {
  content::WebContents* active_contents = GetWebContents();
  DCHECK(active_contents);

  auto* side_search_config =
      SideSearchConfig::Get(active_contents->GetBrowserContext());
  side_search_config->DidShowPageActionLabel();

  ++page_action_label_shown_count_;

  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(active_contents);
  DCHECK(tab_contents_helper);
  tab_contents_helper->DidShowPageActionLabel();
}

void SideSearchIconView::HidePageActionLabel() {
  UnpauseAnimation();
  ResetSlideAnimation(false);
}

BEGIN_METADATA(SideSearchIconView, PageActionIconView)
END_METADATA

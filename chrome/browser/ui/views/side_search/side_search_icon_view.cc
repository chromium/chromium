// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_icon_view.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_metrics.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_search/default_search_icon_source.h"
#include "chrome/browser/ui/views/side_search/side_search_views_utils.h"
#include "chrome/browser/ui/views/side_search/unified_side_search_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

SideSearchIconView::SideSearchIconView(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    Browser* browser)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "SideSearch"),
      browser_(browser),
      icon_changed_subscription_(
          DefaultSearchIconSource::GetOrCreateForBrowser(browser)
              ->RegisterIconChangedSubscription(
                  base::BindRepeating(&SideSearchIconView::UpdateIconImage,
                                      base::Unretained(this)))) {
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(false);
  SetProperty(views::kElementIdentifierKey, kSideSearchButtonElementId);
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_SIDE_SEARCH_ENTRYPOINT_LABEL));
  SetUpForInOutAnimation();
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
  browser_->tab_strip_model()->AddObserver(this);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_TOOLTIP_SIDE_SEARCH_TOOLBAR_BUTTON_NOT_ACTIVATED));
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

bool SideSearchIconView::IsLabelVisibleForTesting() const {
  return label()->GetVisible();
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

  // TODO(crbug.com/40849924): BrowserView should never be null here,
  // investigate why GetBrowserViewForBrowser() is returning null in certain
  // circumstances and remove this check.
  if (!browser_view)
    return;

  const bool was_visible = GetVisible();
  const bool should_show =
      tab_contents_helper->CanShowSidePanelForCommittedNavigation() &&
      !side_search::IsSideSearchToggleOpen(browser_);
  SetVisible(should_show);

  if (should_show && !was_visible) {
    MaybeShowPageActionLabel();
  }

  if (!should_show) {
    HidePageActionLabel();
  }
}

void SideSearchIconView::OnExecuting(PageActionIconView::ExecuteSource source) {
  RecordSideSearchPageActionLabelVisibilityOnToggle(
      label()->GetVisible() ? SideSearchPageActionLabelVisibility::kVisible
                            : SideSearchPageActionLabelVisibility::kNotVisible);

  // Reset the slide animation if in progress.
  HidePageActionLabel();

  SidePanelUI* side_panel_ui = browser_->GetFeatures().side_panel_ui();

  // TODO(crbug.com/40849924): BrowserView should never be null here,
  // investigate why GetBrowserViewForBrowser() is returning null in certain
  // circumstances and remove this check.
  if (!side_panel_ui) {
    return;
  }

  side_panel_ui->Show(
      SidePanelEntry::Id::kSideSearch,
      SidePanelUtil::SidePanelOpenTrigger::kSideSearchPageAction);
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
  return vector_icons::kSearchChromeRefreshIcon;
}

ui::ImageModel SideSearchIconView::GetSizedIconImage(int size) const {
  return DefaultSearchIconSource::GetOrCreateForBrowser(browser_)
      ->GetSizedIconImage(size);
}

void SideSearchIconView::AnimationProgressed(const gfx::Animation* animation) {
  PageActionIconView::AnimationProgressed(animation);
  // When the label is fully revealed pause the animation for
  // kLabelPersistDuration before resuming the animation and allowing the label
  // to animate out. This is currently set to show for 12s including the in/out
  // animation.
  // TODO(crbug.com/40832707): This approach of inspecting the animation
  // progress to extend the animation duration is quite hacky. This should be
  // removed and the IconLabelBubbleView API expanded to support a finer level
  // of control.
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

bool SideSearchIconView::MaybeShowPageActionLabel() {
  auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
      browser_->profile());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

  if (!browser_view || !tracker ||
      !tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHSideSearchPageActionLabelFeature)) {
    return false;
  }

  should_extend_label_shown_duration_ = true;
  AnimateIn(std::nullopt);

  // Note that `Dismiss()` in this case does not dismiss the UI. It's telling
  // the FE backend that the promo is done so that other promos can run. The
  // side panel showing should not block other promos from displaying.
  tracker->Dismissed(feature_engagement::kIPHSideSearchPageActionLabelFeature);
  tracker->NotifyEvent(
      feature_engagement::events::kSideSearchPageActionLabelShown);
  return true;
}

void SideSearchIconView::HidePageActionLabel() {
  UnpauseAnimation();
  ResetSlideAnimation(false);
}

BEGIN_METADATA(SideSearchIconView)
END_METADATA

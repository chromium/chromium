// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_icon_view.h"

#include "base/metrics/user_metrics.h"
#include "base/timer/timer.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

namespace {

// This will add the bookmark to the shopping collection if the feature is
// enabled, otherwise we save to "other bookmarks".
void AddIfNotBookmarkedToTheDefaultFolder(bookmarks::BookmarkModel* model,
                                          content::WebContents* web_contents) {
  GURL url;
  std::u16string title;

  if (chrome::GetURLAndTitleToBookmark(web_contents, &url, &title)) {
    if (bookmarks::IsBookmarkedByUser(model, url)) {
      return;
    }

    const bookmarks::BookmarkNode* parent =
        commerce::GetShoppingCollectionBookmarkFolder(model, true);

    model->AddNewURL(parent, parent->children().size(), title, url);
  }
}

}  // namespace

PriceTrackingIconView::PriceTrackingIconView(
    IconLabelBubbleView::Delegate* parent_delegate,
    Delegate* delegate,
    Browser* browser)
    : PageActionIconView(nullptr,
                         0,
                         parent_delegate,
                         delegate,
                         "PriceTracking"),
      browser_(browser),
      profile_(browser->profile()),
      bubble_coordinator_(this),
      icon_(&omnibox::kPriceTrackingDisabledRefreshIcon) {
  SetUpForInOutAnimation();
  SetProperty(views::kElementIdentifierKey, kPriceTrackingChipElementId);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE));

  SetUseTonalColorsWhenExpanded(
      base::FeatureList::IsEnabled(commerce::kPriceTrackingIconColors));

  if (base::FeatureList::IsEnabled(commerce::kShoppingIconColorVariant)) {
    SetCustomForegroundColorId(kColorShoppingPageActionIconForegroundVariant);
    SetCustomBackgroundColorId(kColorShoppingPageActionIconBackgroundVariant);
  }
}

PriceTrackingIconView::~PriceTrackingIconView() = default;

views::BubbleDialogDelegate* PriceTrackingIconView::GetBubble() const {
  return bubble_coordinator_.GetBubble();
}

void PriceTrackingIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  if (AnimateOutTimer().IsRunning()) {
    AnimateOutTimer().Stop();
  }

  auto* web_contents = GetWebContents();
  DCHECK(web_contents);
  auto* tab_helper = tabs::TabInterface::GetFromContents(web_contents)
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();
  CHECK(tab_helper);

  const gfx::Image& product_image = tab_helper->GetProductImage();
  tab_helper->OnPriceTrackingIconClicked();
  DCHECK(!product_image.IsEmpty());

  base::RecordAction(
      base::UserMetricsAction("Commerce.PriceTracking.OmniboxChipClicked"));

  if (ShouldShowFirstUseExperienceBubble()) {
    bubble_coordinator_.Show(
        GetWebContents(), profile_, GetWebContents()->GetLastCommittedURL(),
        ui::ImageModel::FromImage(product_image),
        base::BindOnce(&PriceTrackingIconView::EnablePriceTracking,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&PriceTrackingIconView::UnpauseAnimation,
                       weak_ptr_factory_.GetWeakPtr()),
        PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE);
  } else {
    EnablePriceTracking(/*enable=*/true);
    bubble_coordinator_.Show(
        GetWebContents(), profile_, GetWebContents()->GetLastCommittedURL(),
        ui::ImageModel::FromImage(product_image),
        base::BindOnce(&PriceTrackingIconView::EnablePriceTracking,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&PriceTrackingIconView::UnpauseAnimation,
                       weak_ptr_factory_.GetWeakPtr()),
        PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);
  }
}

const gfx::VectorIcon& PriceTrackingIconView::GetVectorIcon() const {
  return *icon_;
}

bool PriceTrackingIconView::ShouldShow() {
  if (delegate()->ShouldHidePageActionIcons()) {
    return false;
  }
  auto* web_contents = GetWebContents();
  if (!web_contents)
    return false;
  auto* tab_helper = tabs::TabInterface::GetFromContents(web_contents)
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();

  return tab_helper && tab_helper->ShouldShowPriceTrackingIconView();
}

void PriceTrackingIconView::UpdateImpl() {
  bool should_show = ShouldShow();

  if (should_show) {
    SetVisualState(IsPriceTracking());

    if (!GetVisible()) {
      base::RecordAction(
          base::UserMetricsAction("Commerce.PriceTracking.OmniboxChipShown"));
    }
    MaybeShowPageActionLabel();
  } else {
    HidePageActionLabel();
  }
  SetVisible(should_show);
}

void PriceTrackingIconView::AnimationProgressed(
    const gfx::Animation* animation) {
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
  constexpr base::TimeDelta kLabelPersistDuration = base::Seconds(10.8);
  if (should_extend_label_shown_duration_ &&
      GetAnimationValue() >= kAnimationValueWhenLabelFullyShown) {
    should_extend_label_shown_duration_ = false;
    PauseAnimation();
    AnimateOutTimer().Start(
        FROM_HERE, kLabelPersistDuration,
        base::BindOnce(&PriceTrackingIconView::UnpauseAnimation,
                       base::Unretained(this)));
  }
}

void PriceTrackingIconView::ForceVisibleForTesting(bool is_tracking_price) {
  SetVisible(true);
  SetVisualState(is_tracking_price);
}

const std::u16string& PriceTrackingIconView::GetIconLabelForTesting() {
  return label()->GetText();
}

void PriceTrackingIconView::SetOneShotTimerForTesting(
    base::OneShotTimer* timer) {
  animate_out_timer_for_testing_ = timer;
}

void PriceTrackingIconView::EnablePriceTracking(bool enable) {
  if (IsPriceTracking() == enable)
    return;

  if (enable && ShouldShowFirstUseExperienceBubble()) {
    profile_->GetPrefs()->SetBoolean(prefs::kShouldShowPriceTrackFUEBubble,
                                     false);
  }

  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(profile_);

  const bookmarks::BookmarkNode* existing_node =
      model->GetMostRecentlyAddedUserNodeForURL(
          GetWebContents()->GetLastCommittedURL());
  bool is_new_bookmark = existing_node == nullptr;

  if (enable) {
    AddIfNotBookmarkedToTheDefaultFolder(model, GetWebContents());
    base::RecordAction(
        base::UserMetricsAction("Commerce.PriceTracking.OmniboxChip.Tracked"));
    commerce::MaybeEnableEmailNotifications(profile_->GetPrefs());

    commerce::metrics::RecordShoppingActionUKM(
        GetWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
        commerce::metrics::ShoppingAction::kPriceTracked);
  }

  auto* tab_helper = tabs::TabInterface::GetFromContents(GetWebContents())
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();
  CHECK(tab_helper);

  tab_helper->SetPriceTrackingState(
      enable, is_new_bookmark,
      base::BindOnce(&PriceTrackingIconView::OnPriceTrackingServerStateUpdated,
                     weak_ptr_factory_.GetWeakPtr()));

  SetVisualState(enable);
}

void PriceTrackingIconView::SetVisualState(bool enable) {
    icon_ = enable ? &omnibox::kPriceTrackingEnabledRefreshIcon
                   : &omnibox::kPriceTrackingDisabledRefreshIcon;
  // TODO(meiliang@): Confirm with UXW on the tooltip string. If this expected,
  // we can return label()->GetText() instead.
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        enable ? IDS_OMNIBOX_TRACKING_PRICE : IDS_OMNIBOX_TRACK_PRICE));

    SetLabel(l10n_util::GetStringUTF16(enable ? IDS_OMNIBOX_TRACKING_PRICE
                                              : IDS_OMNIBOX_TRACK_PRICE));
    SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
    UpdateIconImage();
}

void PriceTrackingIconView::OnPriceTrackingServerStateUpdated(bool success) {
  // TODO(crbug.com/40865740): Handles error if |success| is false.
  if (commerce::kRevertIconOnFailure.Get() && !success) {
    bubble_coordinator_.Hide();
    UpdateImpl();
  }
}

bool PriceTrackingIconView::IsPriceTracking() const {
  if (!GetWebContents())
    return false;

  auto* tab_helper = tabs::TabInterface::GetFromContents(GetWebContents())
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();
  CHECK(tab_helper);

  return tab_helper->IsPriceTracking();
}

bool PriceTrackingIconView::ShouldShowFirstUseExperienceBubble() const {
  return profile_->GetPrefs()->GetBoolean(
             prefs::kShouldShowPriceTrackFUEBubble) &&
         !profile_->GetPrefs()->HasPrefPath(
             commerce::kPriceEmailNotificationsEnabled) &&
         !IsPriceTracking();
}

void PriceTrackingIconView::MaybeShowPageActionLabel() {
  if (!base::FeatureList::IsEnabled(commerce::kCommerceAllowChipExpansion)) {
    return;
  }

  auto* tab_helper = tabs::TabInterface::GetFromContents(GetWebContents())
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();

  if (!tab_helper || !tab_helper->ShouldExpandPageActionIcon(
                         PageActionIconType::kPriceTracking)) {
    return;
  }

  should_extend_label_shown_duration_ = true;
  AnimateIn(std::nullopt);
}

void PriceTrackingIconView::HidePageActionLabel() {
  UnpauseAnimation();
  ResetSlideAnimation(false);
}

base::OneShotTimer& PriceTrackingIconView::AnimateOutTimer() {
  return animate_out_timer_for_testing_ ? *animate_out_timer_for_testing_
                                        : animate_out_timer_;
}

BEGIN_METADATA(PriceTrackingIconView)
END_METADATA

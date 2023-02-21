// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
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
#include "ui/base/models/image_model.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view_class_properties.h"

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
      icon_(&omnibox::kPriceTrackingDisabledIcon) {
  SetUpForInOutAnimation();
  SetProperty(views::kElementIdentifierKey, kPriceTrackingChipElementId);
}

PriceTrackingIconView::~PriceTrackingIconView() = default;

views::BubbleDialogDelegate* PriceTrackingIconView::GetBubble() const {
  return bubble_coordinator_.GetBubble();
}

std::u16string PriceTrackingIconView::GetTextForTooltipAndAccessibleName()
    const {
  return tooltip_text_and_accessibleName_;
}

void PriceTrackingIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  if (AnimateOutTimer().IsRunning()) {
    AnimateOutTimer().Stop();
  }

  auto* web_contents = GetWebContents();
  DCHECK(web_contents);
  auto* tab_helper =
      commerce::ShoppingListUiTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);

  const gfx::Image& product_image = tab_helper->GetProductImage();
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
  auto* tab_helper =
      commerce::ShoppingListUiTabHelper::FromWebContents(web_contents);

  return tab_helper && tab_helper->ShouldShowPriceTrackingIconView();
}

void PriceTrackingIconView::UpdateImpl() {
  bool should_show = ShouldShow();

  if (should_show) {
    SetVisualState(IsPriceTracking());

    if (!GetVisible()) {
      base::RecordAction(
          base::UserMetricsAction("Commerce.PriceTracking.OmniboxChipShown"));
      MaybeShowPageActionLabel();
    }
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
  // TODO(crbug.com/1314206): This approach of inspecting the animation progress
  // to extend the animation duration is quite hacky. This should be removed and
  // the IconLabelBubbleView API expanded to support a finer level of control.
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
    GURL url;
    std::u16string title;
    if (chrome::GetURLAndTitleToBookmark(GetWebContents(), &url, &title)) {
      bookmarks::AddIfNotBookmarked(model, url, title);
    }
    base::RecordAction(
        base::UserMetricsAction("Commerce.PriceTracking.OmniboxChip.Tracked"));
    commerce::MaybeEnableEmailNotifications(profile_->GetPrefs());
    bool should_show_iph = browser_->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHPriceTrackingInSidePanelFeature);
    if (should_show_iph) {
      SidePanelCoordinator* coordinator =
          BrowserView::GetBrowserViewForBrowser(browser_)
              ->side_panel_coordinator();
      if (coordinator) {
        SidePanelRegistry* registry =
            SidePanelCoordinator::GetGlobalSidePanelRegistry(browser_);
        registry->SetActiveEntry(registry->GetEntryForKey(
            SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks)));
      } else {
        profile_->GetPrefs()->SetBoolean(prefs::kShouldShowSidePanelBookmarkTab,
                                         true);
      }
    }
  }

  const bookmarks::BookmarkNode* node =
      existing_node ? existing_node
                    : model->GetMostRecentlyAddedUserNodeForURL(
                          GetWebContents()->GetLastCommittedURL());

  commerce::ShoppingService* service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile_);
  base::OnceCallback<void(bool)> callback =
      base::BindOnce(&PriceTrackingIconView::OnPriceTrackingServerStateUpdated,
                     weak_ptr_factory_.GetWeakPtr());

  if (node) {
    commerce::SetPriceTrackingStateForBookmark(
        commerce::ShoppingServiceFactory::GetForBrowserContext(profile_), model,
        node, enable, std::move(callback), enable && is_new_bookmark);
  } else {
    DCHECK(!enable);
    absl::optional<commerce::ProductInfo> info =
        service->GetAvailableProductInfoForUrl(
            GetWebContents()->GetLastCommittedURL());
    if (info.has_value()) {
      commerce::SetPriceTrackingStateForClusterId(
          commerce::ShoppingServiceFactory::GetForBrowserContext(profile_),
          model, info->product_cluster_id, enable, std::move(callback));
    }
  }

  SetVisualState(enable);
}

void PriceTrackingIconView::SetVisualState(bool enable) {
  icon_ = enable ? &omnibox::kPriceTrackingEnabledFilledIcon
                 : &omnibox::kPriceTrackingDisabledIcon;
  // TODO(meiliang@): Confirm with UXW on the tooltip string. If this expected,
  // we can return label()->GetText() instead.
  tooltip_text_and_accessibleName_ = l10n_util::GetStringUTF16(
      enable ? IDS_OMNIBOX_TRACKING_PRICE : IDS_OMNIBOX_TRACK_PRICE);

  SetLabel(l10n_util::GetStringUTF16(enable ? IDS_OMNIBOX_TRACKING_PRICE
                                            : IDS_OMNIBOX_TRACK_PRICE));
  SetPaintLabelOverSolidBackground(true);
  UpdateIconImage();
}

void PriceTrackingIconView::OnPriceTrackingServerStateUpdated(bool success) {
  // TODO(crbug.com/1364739): Handles error if |success| is false.
  if (commerce::kRevertIconOnFailure.Get() && !success) {
    bubble_coordinator_.Hide();
    UpdateImpl();
  }
}

bool PriceTrackingIconView::IsPriceTracking() const {
  if (!GetWebContents())
    return false;

  auto* tab_helper =
      commerce::ShoppingListUiTabHelper::FromWebContents(GetWebContents());
  CHECK(tab_helper);

  return tab_helper->IsPriceTracking();
}

bool PriceTrackingIconView::ShouldShowFirstUseExperienceBubble() const {
  return profile_->GetPrefs()->GetBoolean(
             prefs::kShouldShowPriceTrackFUEBubble) &&
         !IsPriceTracking();
}

void PriceTrackingIconView::MaybeShowPageActionLabel() {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
  if (!tracker ||
      !tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHPriceTrackingPageActionIconLabelFeature)) {
    return;
  }

  should_extend_label_shown_duration_ = true;
  AnimateIn(absl::nullopt);

  // Note that `Dismiss()` in this case does not dismiss the UI. It's telling
  // the FE backend that the promo is done so that other promos can run. Showing
  // the label should not block other promos from displaying.
  tracker->Dismissed(
      feature_engagement::kIPHPriceTrackingPageActionIconLabelFeature);
}

void PriceTrackingIconView::HidePageActionLabel() {
  UnpauseAnimation();
  ResetSlideAnimation(false);
}

base::OneShotTimer& PriceTrackingIconView::AnimateOutTimer() {
  return animate_out_timer_for_testing_ ? *animate_out_timer_for_testing_
                                        : animate_out_timer_;
}

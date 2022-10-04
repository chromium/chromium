// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_icon_view.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view_class_properties.h"

PriceTrackingIconView::PriceTrackingIconView(
    IconLabelBubbleView::Delegate* parent_delegate,
    Delegate* delegate,
    Profile* profile)
    : PageActionIconView(nullptr,
                         0,
                         parent_delegate,
                         delegate,
                         "PriceTracking"),
      profile_(profile),
      bubble_coordinator_(this),
      icon_(&omnibox::kPriceTrackingDisabledIcon) {
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
  if (profile_->GetPrefs()->GetBoolean(prefs::kShouldShowPriceTrackFUEBubble)) {
    bubble_coordinator_.Show(
        GetWebContents(), profile_, GetWebContents()->GetLastCommittedURL(),

        base::BindOnce(&PriceTrackingIconView::EnablePriceTracking,
                       base::Unretained(this)),
        PriceTrackingBubbleDialogView::Type::TYPE_FUE);
  } else {
    EnablePriceTracking(/*enable=*/true);
    bubble_coordinator_.Show(
        GetWebContents(), profile_, GetWebContents()->GetLastCommittedURL(),
        base::BindOnce(&PriceTrackingIconView::EnablePriceTracking,
                       base::Unretained(this)),
        PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);
  }
}

const gfx::VectorIcon& PriceTrackingIconView::GetVectorIcon() const {
  return *icon_;
}

void PriceTrackingIconView::UpdateImpl() {
  SetVisualState(IsPriceTracking());
  SetVisible(is_visible_);
}

void PriceTrackingIconView::ForceVisibleForTesting(bool is_tracking_price) {
  SetVisible(true);
  is_visible_ = true;
  SetVisualState(is_tracking_price);
}

const std::u16string& PriceTrackingIconView::GetIconLabelForTesting() {
  return label()->GetText();
}

void PriceTrackingIconView::EnablePriceTracking(bool enable) {
  if (IsPriceTracking() == enable)
    return;

  if (enable &&
      profile_->GetPrefs()->GetBoolean(prefs::kShouldShowPriceTrackFUEBubble)) {
    profile_->GetPrefs()->SetBoolean(prefs::kShouldShowPriceTrackFUEBubble,
                                     false);
  }

  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  const bookmarks::BookmarkNode* node =
      model->GetMostRecentlyAddedUserNodeForURL(
          GetWebContents()->GetLastCommittedURL());
  commerce::SetPriceTrackingStateForBookmark(
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile_), model,
      node, enable,
      base::BindOnce(&PriceTrackingIconView::OnPriceTrackingServerStateUpdated,
                     weak_ptr_factory_.GetWeakPtr()));

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
}

bool PriceTrackingIconView::IsPriceTracking() const {
  if (!GetWebContents()) {
    return false;
  }
  bookmarks::BookmarkModel* const bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  const bookmarks::BookmarkNode* bookmark_node =
      bookmark_model->GetMostRecentlyAddedUserNodeForURL(
          GetWebContents()->GetLastCommittedURL());
  return commerce::IsBookmarkPriceTracked(bookmark_model, bookmark_node);
}

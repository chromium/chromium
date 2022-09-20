// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_icon_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/common/pref_names.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
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
      bubble_coordinator_(this) {
  SetProperty(views::kElementIdentifierKey, kPriceTrackingChipElementId);
}

views::BubbleDialogDelegate* PriceTrackingIconView::GetBubble() const {
  return bubble_coordinator_.GetBubble();
}

std::u16string PriceTrackingIconView::GetTextForTooltipAndAccessibleName()
    const {
  // TODO(meiliang@): Confirm with UXW on the tooltip string.
  return l10n_util::GetStringUTF16(is_tracking_price_
                                       ? IDS_OMNIBOX_TRACKING_PRICE
                                       : IDS_OMNIBOX_TRACK_PRICE);
}

void PriceTrackingIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  if (profile_->GetPrefs()->GetBoolean(prefs::kShouldShowPriceTrackFUEBubble)) {
    bubble_coordinator_.Show(GetWebContents(),
                             base::BindOnce(&PriceTrackingIconView::TrackPrice,
                                            base::Unretained(this)),
                             PriceTrackingBubbleDialogView::Type::TYPE_FUE);
  } else {
    bubble_coordinator_.Show(GetWebContents(),
                             base::BindOnce(&PriceTrackingIconView::TrackPrice,
                                            base::Unretained(this)),
                             PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);

    // TODO(meiliang@): Call enable price tracking util and use UpdateImpl() as
    // the callback if the page is not price tracked yet.
  }
}

const gfx::VectorIcon& PriceTrackingIconView::GetVectorIcon() const {
  return is_tracking_price_ ? omnibox::kPriceTrackingEnabledFilledIcon
                            : omnibox::kPriceTrackingDisabledIcon;
}

void PriceTrackingIconView::UpdateImpl() {
  SetLabel(l10n_util::GetStringUTF16(is_tracking_price_
                                         ? IDS_OMNIBOX_TRACKING_PRICE
                                         : IDS_OMNIBOX_TRACK_PRICE));
  SetPaintLabelOverSolidBackground(true);
  SetVisible(is_visible_);
  UpdateIconImage();
}

void PriceTrackingIconView::ForceVisibleForTesting(bool is_tracking_price) {
  is_visible_ = true;
  is_tracking_price_ = is_tracking_price;
  UpdateImpl();
}

void PriceTrackingIconView::TrackPrice() {
  if (profile_->GetPrefs()->GetBoolean(prefs::kShouldShowPriceTrackFUEBubble)) {
    profile_->GetPrefs()->SetBoolean(prefs::kShouldShowPriceTrackFUEBubble,
                                     false);
  }
  // TODO(meiliang@): Call commerce::SetPriceTrackingStateForBookmark
}

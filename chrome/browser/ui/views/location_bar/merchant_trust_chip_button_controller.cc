// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/merchant_trust_chip_button_controller.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MerchantTrustChipButtonController,
                                      kElementIdForTesting);

MerchantTrustChipButtonController::MerchantTrustChipButtonController(
    OmniboxChipButton* chip_button,
    LocationIconView* location_icon_view)
    : chip_button_(chip_button), location_icon_view_(location_icon_view) {
  // TODO(crbug.com/378854462): Revisit icons, strings and theme.
  chip_button_->SetIcon(vector_icons::kStorefrontIcon);
  chip_button_->SetText(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_MERCHANT_TRUST_HEADER));
  chip_button_->SetTheme(OmniboxChipTheme::kLowVisibility);
  chip_button_->SetCallback(base::BindRepeating(
      &MerchantTrustChipButtonController::OpenPageInfoSubpage,
      base::Unretained(this)));
  chip_button_->SetProperty(views::kElementIdentifierKey, kElementIdForTesting);
}

MerchantTrustChipButtonController::~MerchantTrustChipButtonController() =
    default;

void MerchantTrustChipButtonController::UpdateWebContents(
    content::WebContents* web_contents) {
  web_contents_ = web_contents->GetWeakPtr();

  // TODO(crbug.com/378854906): Fetch the data.
}

void MerchantTrustChipButtonController::Show() {
  const int radius = GetLayoutConstant(LOCATION_BAR_CHILD_CORNER_RADIUS);
  location_icon_view_->SetCornerRadii(
      gfx::RoundedCornersF(radius, 0, 0, radius));
  chip_button_->SetCornerRadii(gfx::RoundedCornersF(0, radius, radius, 0));
  chip_button_->SetVisible(true);

  // TODO(crbug.com/378854906): Animate expand.
}

void MerchantTrustChipButtonController::Hide() {
  location_icon_view_->SetCornerRadii(gfx::RoundedCornersF(
      location_icon_view_->GetPreferredSize().height() / 2));
  chip_button_->SetVisible(false);

  // TODO(crbug.com/378854906): Animate collapse.
}

void MerchantTrustChipButtonController::OpenPageInfoSubpage() {
  if (!web_contents_) {
    return;
  }

  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  if (entry->IsInitialEntry()) {
    return;
  }

  auto initialized_callback =
      GetPageInfoDialogCreatedCallbackForTesting()
          ? std::move(GetPageInfoDialogCreatedCallbackForTesting())
          : base::DoNothing();

  // TODO(crbug.com/378854462): Prevent bubble from reopening when clicking on
  // the button while the bubble is open. Anchor by the main location bar icon
  // and set chip_button_ as highlighted button.
  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          location_icon_view_, gfx::Rect(),
          chip_button_->GetWidget()->GetNativeWindow(), web_contents_.get(),
          entry->GetVirtualURL(), std::move(initialized_callback),
          base::DoNothing(),
          /*allow_about_this_site=*/true, std::nullopt, true);
  bubble->SetHighlightedButton(chip_button_);
  bubble->GetWidget()->Show();
}

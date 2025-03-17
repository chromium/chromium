// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/merchant_trust_chip_button_controller.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/page_info/core/merchant_trust_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_class_properties.h"

namespace {

// A duration of the expand animation.
constexpr auto kExpandAnimationDuration = base::Milliseconds(350);
// A duration of the collapse animation.
constexpr auto kCollapseAnimationDuration = base::Milliseconds(250);
// A delay before collapsing an expanded chip.
constexpr auto kCollapseDelay = base::Seconds(12);

base::TimeDelta GetAnimationDuration(base::TimeDelta duration) {
  return gfx::Animation::ShouldRenderRichAnimation() ? duration
                                                     : base::TimeDelta();
}

}  // namespace

MerchantTrustChipButtonController::MerchantTrustChipButtonController(
    OmniboxChipButton* chip_button,
    LocationIconView* location_icon_view,
    page_info::MerchantTrustService* service)
    : chip_button_(chip_button),
      location_icon_view_(location_icon_view),
      service_(service) {
  // TODO(crbug.com/378854462): Revisit icons, strings and theme.
  chip_button_->SetIcon(vector_icons::kStorefrontIcon);
  chip_button_->SetText(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_MERCHANT_TRUST_HEADER));
  chip_button_->SetTheme(OmniboxChipTheme::kLowVisibility);
  chip_button_->SetCallback(base::BindRepeating(
      &MerchantTrustChipButtonController::OpenPageInfoSubpage,
      weak_factory_.GetWeakPtr()));
  chip_button_->SetProperty(views::kElementIdentifierKey,
                            kMerchantTrustChipElementId);
}

MerchantTrustChipButtonController::~MerchantTrustChipButtonController() =
    default;

void MerchantTrustChipButtonController::UpdateWebContents(
    content::WebContents* contents) {
  if (contents && contents != web_contents()) {
    Observe(contents);
    // Fetch the data when the web contents changes. `PrimaryPageChanged()` only
    // covers changes to the currently observed web contents.
    FetchData();
  }
}

void MerchantTrustChipButtonController::PrimaryPageChanged(
    content::Page& page) {
  FetchData();
}

void MerchantTrustChipButtonController::FetchData() {
  DCHECK(service_);
  service_->GetMerchantTrustInfo(
      web_contents()->GetVisibleURL(),
      base::BindOnce(
          &MerchantTrustChipButtonController::OnMerchantTrustDataFetched,
          weak_factory_.GetWeakPtr()));
}

void MerchantTrustChipButtonController::OnMerchantTrustDataFetched(
    const GURL& url,
    std::optional<page_info::MerchantData> merchant_data) {
  merchant_data_ = merchant_data;

  if (ShouldBeVisible()) {
    // Reset if animation is in progress. Animate expand when visibility
    // changes.
    chip_button_->ResetAnimation(0.0);
    if (!chip_button_->GetVisible()) {
      if (!web_contents()->GetUserData(kChipAnimated)) {
        chip_button_->AnimateExpand(
            GetAnimationDuration(kExpandAnimationDuration));
        web_contents()->SetUserData(
            kChipAnimated, std::make_unique<base::SupportsUserData::Data>());
      }
    }
    Show();
    StartCollapseTimer();
  } else {
    // Don't animate collapse because the chip gets hidden instantly when
    // switching tabs.
    Hide();
  }
}

bool MerchantTrustChipButtonController::ShouldBeVisible() {
  return merchant_data_.has_value();
}

void MerchantTrustChipButtonController::Show() {
  const int radius = GetLayoutConstant(LOCATION_BAR_CHILD_CORNER_RADIUS);
  location_icon_view_->SetCornerRadii(
      gfx::RoundedCornersF(radius, 0, 0, radius));
  chip_button_->SetCornerRadii(gfx::RoundedCornersF(0, radius, radius, 0));
  chip_button_->SetVisible(true);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MerchantTrustChipButtonController::
                         MaybeShowMerchantTrustFeaturePromo,
                     weak_factory_.GetWeakPtr()),
      kExpandAnimationDuration);
}

void MerchantTrustChipButtonController::Hide() {
  location_icon_view_->SetCornerRadii(gfx::RoundedCornersF(
      location_icon_view_->GetPreferredSize().height() / 2));
  chip_button_->SetVisible(false);
}

void MerchantTrustChipButtonController::StartCollapseTimer() {
  collapse_timer_.Start(
      FROM_HERE, GetAnimationDuration(kCollapseDelay),
      base::BindOnce(&MerchantTrustChipButtonController::Collapse,
                     weak_factory_.GetWeakPtr()));
}

void MerchantTrustChipButtonController::Collapse() {
  chip_button_->AnimateCollapse(
      GetAnimationDuration(kCollapseAnimationDuration));
}

void MerchantTrustChipButtonController::OpenPageInfoSubpage() {
  if (!web_contents()) {
    return;
  }

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
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
          chip_button_->GetWidget()->GetNativeWindow(), web_contents(),
          entry->GetVirtualURL(), std::move(initialized_callback),
          base::DoNothing(),
          /*allow_extended_site_info=*/true, std::nullopt, true);
  bubble->SetHighlightedButton(chip_button_);
  bubble->GetWidget()->Show();
}

void MerchantTrustChipButtonController::MaybeShowMerchantTrustFeaturePromo() {
  if (!web_contents()) {
    return;
  }

  if (auto* interface =
          BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
              web_contents())) {
    bool can_show_promo = interface->CanShowFeaturePromo(
        feature_engagement::kIPHMerchantTrustFeature);
    if (can_show_promo) {
      interface->MaybeShowFeaturePromo(
          feature_engagement::kIPHMerchantTrustFeature);
    }
  }
}

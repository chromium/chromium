// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/merchant_trust_chip_button_controller.h"

#include <optional>

#include "base/functional/bind.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

MerchantTrustChipButtonController::MerchantTrustChipButtonController(
    OmniboxChipButton* chip_button,
    LocationIconView* location_icon_view)
    : chip_button_(chip_button), location_icon_view_(location_icon_view) {
  // TODO(crbug.com/378854462): Revisit icons, strings and theme.
  chip_button_->SetText(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_MERCHANT_TRUST_HEADER));
  chip_button_->SetTheme(OmniboxChipTheme::kLowVisibility);
  chip_button_->SetCallback(base::BindRepeating(
      &MerchantTrustChipButtonController::OpenPageInfoSubpage,
      base::Unretained(this)));
}

MerchantTrustChipButtonController::~MerchantTrustChipButtonController() =
    default;

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
  // TODO(crbug.com/378854462): Open page info.
}

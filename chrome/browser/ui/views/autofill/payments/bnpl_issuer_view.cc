// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_issuer_view.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "bnpl_issuer_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/grit/components_scaled_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"

namespace autofill::payments {

BnplIssuerView::BnplIssuerView(
    base::WeakPtr<SelectBnplIssuerDialogController> controller)
    : controller_(controller) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  auto issuers = controller_->GetIssuers();
  for (auto issuer : issuers) {
    int image_id = IDR_AUTOFILL_AFFIRM_UNLINKED;
    if (issuer.issuer_id() == kBnplZipIssuerId) {
      image_id = issuer.payment_instrument().has_value()
                     ? IDR_AUTOFILL_ZIP_LINKED
                     : IDR_AUTOFILL_ZIP_UNLINKED;
    } else if (issuer.issuer_id() == kBnplAffirmIssuerId) {
      image_id = issuer.payment_instrument().has_value()
                     ? IDR_AUTOFILL_AFFIRM_LINKED
                     : IDR_AUTOFILL_AFFIRM_UNLINKED;
    } else if (issuer.issuer_id() == kBnplAfterpayIssuerId) {
      image_id = issuer.payment_instrument().has_value()
                     ? IDR_AUTOFILL_AFTERPAY_LINKED
                     : IDR_AUTOFILL_AFTERPAY_UNLINKED;
    }
    auto image_view =
        std::make_unique<views::ImageView>(ui::ImageModel::FromImageSkia(
            *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                image_id)));
    auto issuer_button = std::make_unique<HoverButton>(
        views::Button::PressedCallback(base::BindRepeating(
            &BnplIssuerView::IssuerSelected, base::Unretained(this), issuer)),
        std::move(image_view), std::u16string(issuer.GetDisplayName()),
        // TODO(crbug.com/356443046): Move to resources and translate string.
        u"Pay monthly or in 4 intrest-free installments (subject to "
        u"eligibility)",
        nullptr, true, std::u16string(),
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_LABEL_HORIZONTAL),
        true);
    AddChildView(std::move(issuer_button));
  }
}

BnplIssuerView::~BnplIssuerView() = default;

void BnplIssuerView::AddedToWidget() {
  views::BoxLayoutView::AddedToWidget();
  SkColor background_color =
      GetColorProvider()->GetColor(ui::kColorDialogBackground);
  for (auto child : children()) {
    if (auto* issuer_button = views::AsViewClass<HoverButton>(child)) {
      issuer_button->SetTitleTextStyle(views::style::STYLE_EMPHASIZED,
                                       background_color, std::nullopt);
    }
  }
}

void BnplIssuerView::IssuerSelected(BnplIssuer issuer, const ui::Event& event) {
  if (controller_) {
    controller_->OnAccepted(std::string(issuer.issuer_id()));
    controller_->OnDialogClosed();
  }
}

BEGIN_METADATA(BnplIssuerView)
END_METADATA

}  // namespace autofill::payments

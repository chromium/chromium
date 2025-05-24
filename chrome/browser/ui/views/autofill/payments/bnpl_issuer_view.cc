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
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/autofill/payments/bnpl_issuer_linked_pill.h"
#include "chrome/browser/ui/views/autofill/payments/select_bnpl_issuer_dialog.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/grit/components_scaled_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/theme_tracking_image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace autofill::payments {

using IssuerId = autofill::BnplIssuer::IssuerId;

BnplIssuerView::BnplIssuerView(
    base::WeakPtr<SelectBnplIssuerDialogController> controller,
    SelectBnplIssuerDialog* issuer_dialog)
    : issuer_dialog_(issuer_dialog), controller_(controller) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
}

BnplIssuerView::~BnplIssuerView() = default;

void BnplIssuerView::AddedToWidget() {
  views::BoxLayoutView::AddedToWidget();
  auto* layout_provider = ChromeLayoutProvider::Get();
  SetBetweenChildSpacing(layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL));
  int corner_radius =
      layout_provider->GetCornerRadiusMetric(views::Emphasis::kHigh);
  auto issuer_contexts = controller_->GetIssuerContexts();
  for (const auto& [issuer, eligibility] : issuer_contexts) {
    bool issuer_eligible =
        eligibility == BnplIssuerEligibilityForPage::kIsEligible;
    const bool issuer_linked = issuer.payment_instrument().has_value();
    const auto image_ids = [&]() -> std::pair<int, int> {
      if (issuer_linked) {
        switch (issuer.issuer_id()) {
          case IssuerId::kBnplAffirm:
            return {IDR_AUTOFILL_AFFIRM_LINKED,
                    IDR_AUTOFILL_AFFIRM_LINKED_DARK};
          case IssuerId::kBnplZip:
            return {IDR_AUTOFILL_ZIP_LINKED, IDR_AUTOFILL_ZIP_LINKED_DARK};
          case IssuerId::kBnplAfterpay:
            return {IDR_AUTOFILL_AFTERPAY_LINKED,
                    IDR_AUTOFILL_AFTERPAY_LINKED_DARK};
        }
        NOTREACHED();
      }
      switch (issuer.issuer_id()) {
        case IssuerId::kBnplAffirm:
          return {IDR_AUTOFILL_AFFIRM_UNLINKED,
                  IDR_AUTOFILL_AFFIRM_UNLINKED_DARK};
        case IssuerId::kBnplZip:
          return {IDR_AUTOFILL_ZIP_UNLINKED, IDR_AUTOFILL_ZIP_UNLINKED_DARK};
        case IssuerId::kBnplAfterpay:
          return {IDR_AUTOFILL_AFTERPAY_UNLINKED,
                  IDR_AUTOFILL_AFTERPAY_UNLINKED_DARK};
      }
      NOTREACHED();
    }();

    auto image_view = std::make_unique<views::ThemeTrackingImageView>(
        ui::ImageModel::FromResourceId(image_ids.first),
        ui::ImageModel::FromResourceId(image_ids.second),
        base::BindRepeating(
            [](views::View* view) {
              return ui::ColorVariant(view->GetColorProvider()->GetColor(
                  ui::kColorDialogBackground));
            },
            base::Unretained(this)));
    auto* image_view_ptr = image_view.get();
    auto issuer_button = std::make_unique<HoverButton>(
        views::Button::PressedCallback(base::BindRepeating(
            &BnplIssuerView::IssuerSelected, base::Unretained(this), issuer)),
        std::move(image_view), std::u16string(issuer.GetDisplayName()),
        controller_->GetSelectionOptionText(issuer.issuer_id()), nullptr, true,
        std::u16string(),
        layout_provider->GetDistanceMetric(
            views::DISTANCE_RELATED_LABEL_HORIZONTAL),
        true);
    issuer_button->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(layout_provider->GetDistanceMetric(
                            views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                        layout_provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_LABEL_HORIZONTAL))));
    issuer_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    // Make the highlight with rounded corners per the mocks.
    if (auto* ink_drop = views::InkDrop::Get(issuer_button.get())) {
      ink_drop->SetCreateHighlightCallback(base::BindRepeating(
          [](HoverButton* issuer_button, int corner_radius) {
            auto highlight = std::make_unique<views::InkDropHighlight>(
                issuer_button->size(), corner_radius,
                gfx::RectF(issuer_button->GetMirroredRect(
                               issuer_button->GetLocalBounds()))
                    .CenterPoint(),
                views::InkDrop::Get(issuer_button)->GetBaseColor());
            highlight->set_visible_opacity(1.0f);
            return highlight;
          },
          base::Unretained(issuer_button.get()), corner_radius));
      ink_drop->SetSmallCornerRadius(corner_radius);
      ink_drop->SetLargeCornerRadius(corner_radius);
    }
    BnplLinkedIssuerPill* linked_pill = nullptr;
    if (issuer_linked) {
      issuer_button->AddChildView(
          views::Builder<BnplLinkedIssuerPill>()
              .CopyAddressTo(&linked_pill)
              .SetProperty(views::kMarginsKey,
                           gfx::Insets::TLBR(
                               0,
                               layout_provider->GetDistanceMetric(
                                   views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                               0, 0))
              .Build());
      issuer_button->GetViewAccessibility().SetDescription(
          linked_pill->GetAccessibilityDescription());
    }
    issuer_button->AddChildView(
        views::Builder<views::ImageView>()
            .SetImage(ui::ImageModel::FromVectorIcon(
                kChevronRightChromeRefreshIcon,
                issuer_eligible ? kColorBnplIssuerLabelForeground
                                : kColorBnplIssuerLabelForegroundDisabled))
            .SetProperty(views::kMarginsKey,
                         gfx::Insets::TLBR(
                             0,
                             layout_provider->GetDistanceMetric(
                                 views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                             0, 0))
            .Build());
    if (!issuer_eligible) {
      issuer_button->SetEnabled(false);
      if (issuer_linked) {
        issuer_button->SetBackground(views::CreateRoundedRectBackground(
            kColorBnplIssuerLinkedIneligibleBackground, corner_radius));
      }
      image_view_ptr->SetPaintToLayer();
      image_view_ptr->layer()->SetOpacity(0.38f);  // 35% opacity.
      if (linked_pill) {
        linked_pill->SetPaintToLayer();
        linked_pill->layer()->SetOpacity(0.38f);
      }
    }
    views::SetCascadingColorProviderColor(
        issuer_button.get(), views::kCascadingLabelEnabledColor,
        issuer_eligible ? kColorBnplIssuerLabelForeground
                        : kColorBnplIssuerLabelForegroundDisabled);
    AddChildView(std::move(issuer_button));
  }

  // TODO (crbug.com/402646513): Update color token to use a context-specific
  // token.
  SkColor background_color =
      GetColorProvider()->GetColor(ui::kColorDialogBackground);
  for (auto child : children()) {
    if (auto* issuer_button = views::AsViewClass<HoverButton>(child)) {
      issuer_button->SetTitleTextStyle(views::style::STYLE_EMPHASIZED,
                                       background_color, std::nullopt);
    }
  }
  CHECK(!children().empty());
  children()[0]->RequestFocus();
}

void BnplIssuerView::IssuerSelected(BnplIssuer issuer, const ui::Event& event) {
  if (controller_) {
    issuer_dialog_->DisplayThrobber();
    controller_->OnIssuerSelected(std::move(issuer));
  }
}

BEGIN_METADATA(BnplIssuerView)
END_METADATA

}  // namespace autofill::payments

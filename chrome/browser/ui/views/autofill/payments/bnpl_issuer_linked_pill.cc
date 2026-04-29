// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_issuer_linked_pill.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace autofill::payments {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BnplLinkedIssuerPill,
                                      kBnplLinkedPillElementId);

BnplLinkedIssuerPill::BnplLinkedIssuerPill()
    : views::Label(l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_CARD_BNPL_LINKED_ISSUER_PILL_LABEL),
                   views::style::CONTEXT_DIALOG_BODY_TEXT,
                   views::style::STYLE_PRIMARY) {
  SetEnabledColor(kColorBnplIssuerLinkedPillForeground);
  SetProperty(views::kElementIdentifierKey, kBnplLinkedPillElementId);
}

BnplLinkedIssuerPill::~BnplLinkedIssuerPill() = default;

void BnplLinkedIssuerPill::AddedToWidget() {
  views::Label::AddedToWidget();
  gfx::Insets border_insets;
  // TODO(crbug.com/507870463): Find appropriate metrics on ChromeLayoutProvider
  // for size calculation.
  gfx::Size size;

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs)) {
    // Use the style and size config for pop up badges in the Pay Later tab.
    SetTextStyle(kPopupBadgeTextStyle);
    size = GetPreferredSize(views::SizeBounds());
    border_insets = kPopupBadgeBorderInsets;
    size.Enlarge(0, border_insets.height());
  } else {
    // Calculate label border based on label size for issuer selection dialog.
    size = GetPreferredSize(views::SizeBounds());
    size.Enlarge(0, 4);
    border_insets =
        gfx::Insets::TLBR(2, size.height() / 2, 2, size.height() / 2);
  }

  SetBackground(
      views::CreateSolidBackground(kColorBnplIssuerLinkedPillBackground));
  SetBorder(views::CreateRoundedRectBorder(
      /*thickness=*/0, /*corner_radius=*/size.height() / 2, border_insets,
      kColorBnplIssuerLinkedPillBackground));
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(size.height() / 2));

  if (!GetEnabled()) {
    layer()->SetOpacity(0.38f);
  }
}

std::u16string BnplLinkedIssuerPill::GetAccessibilityDescription() {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_BNPL_LINKED_ISSUER_PILL_LABEL);
}

BEGIN_METADATA(BnplLinkedIssuerPill)
END_METADATA

}  // namespace autofill::payments

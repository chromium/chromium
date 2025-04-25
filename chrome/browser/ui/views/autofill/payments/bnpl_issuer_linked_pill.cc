// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_issuer_linked_pill.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/strings/grit/components_strings.h"
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

namespace autofill::payments {

BnplLinkedIssuerPill::BnplLinkedIssuerPill()
    : views::Label(l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_CARD_BNPL_LINKED_ISSUER_PILL_LABEL),
                   views::style::CONTEXT_DIALOG_BODY_TEXT,
                   views::style::STYLE_PRIMARY) {
  SetEnabledColor(kColorBnplIssuerLinkedPillForeground);
}

BnplLinkedIssuerPill::~BnplLinkedIssuerPill() = default;

void BnplLinkedIssuerPill::AddedToWidget() {
  views::Label::AddedToWidget();
  // TODO(kylixrd): Find appropriate metrics on ChromeLayoutProvider.
  gfx::Size size = GetPreferredSize(views::SizeBounds());
  size.Enlarge(0, 4);
  SetBackground(
      views::CreateSolidBackground(kColorBnplIssuerLinkedPillBackground));
  SetBorder(views::CreateRoundedRectBorder(
      0, size.height() / 2,
      gfx::Insets::TLBR(2, size.height() / 2, 2, size.height() / 2),
      kColorBnplIssuerLinkedPillBackground));
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(size.height() / 2));
}

std::u16string BnplLinkedIssuerPill::GetAccessibilityDescription() {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_BNPL_LINKED_ISSUER_PILL_LABEL);
}

BEGIN_METADATA(BnplLinkedIssuerPill)
END_METADATA

}  // namespace autofill::payments

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_issuer_linked_pill.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/style/typography.h"

namespace autofill::payments {

BnplLinkedIssuerPill::BnplLinkedIssuerPill()
    // TODO(crbug.com/356443046): Move to resources and translate string.
    : views::Label(u"Linked",
                   views::style::CONTEXT_DIALOG_BODY_TEXT,
                   views::style::STYLE_SECONDARY) {
  // TODO(kylixrd): Find appropriate metrics on ChromeLayoutProvider.
  // TODO (crbug.com/402646513): Update color token to use a context-specific
  // token.
  SetBackground(views::CreateRoundedRectBackground(ui::kColorBadgeBackground,
                                                   gfx::RoundedCornersF(8)));
  SetBorder(views::CreateRoundedRectBorder(0, 8, gfx::Insets::TLBR(0, 4, 0, 4),
                                           ui::kColorBadgeBackground));
}

BnplLinkedIssuerPill::~BnplLinkedIssuerPill() = default;

BEGIN_METADATA(BnplLinkedIssuerPill)
END_METADATA

}  // namespace autofill::payments

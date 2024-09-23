// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_beta_badge.h"

#include <string>

#include "base/i18n/rtl.h"
#include "cc/paint/paint_flags.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {
// Padding that appears around the "Beta" label.
gfx::Outsets kInternalPadding = gfx::Outsets::VH(4, 10);

// The corners of the label are rounded,
int kCornerRadius = 10;

// Colors used by the badge.
ui::ColorId kTextColor = cros_tokens::kCrosSysOnPrimaryContainer;
ui::ColorId kBackgroundColor = cros_tokens::kCrosSysHighlightShape;

gfx::FontList GetFont() {
  // TODO(b/284389804): Use TypographyToken::kCrosButton1
  return gfx::FontList({"Google Sans", "Roboto"}, gfx::Font::NORMAL, 14,
                       gfx::Font::Weight::MEDIUM);
}

}  // namespace

BorealisBetaBadge::BorealisBetaBadge() {
  GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  GetViewAccessibility().SetName(GetText());
}

BorealisBetaBadge::~BorealisBetaBadge() = default;

std::u16string BorealisBetaBadge::GetText() const {
  return l10n_util::GetStringUTF16(IDS_BOREALIS_BETA_BADGE);
}

gfx::Size BorealisBetaBadge::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  gfx::Rect preferred(gfx::GetStringSize(GetText(), GetFont()));
  preferred.Outset(kInternalPadding);
  return preferred.size();
}

void BorealisBetaBadge::OnPaint(gfx::Canvas* canvas) {
  gfx::Size text_size = gfx::GetStringSize(GetText(), GetFont());
  // The text is offset from the top-left corner by the inset amount
  gfx::Rect badge_text_bounds{
      gfx::Point(kInternalPadding.left(), kInternalPadding.top()), text_size};
  // ...depending on the side text is written on.
  if (base::i18n::IsRTL()) {
    badge_text_bounds.set_x(GetMirroredXForRect(badge_text_bounds));
  }

  const ui::ColorProvider* color_provider = GetColorProvider();

  // Render the badge itself.
  gfx::Rect surrounding(badge_text_bounds);
  surrounding.Outset(kInternalPadding);
  cc::PaintFlags flags;
  flags.setColor(color_provider->GetColor(kBackgroundColor));
  canvas->DrawRoundRect(surrounding, kCornerRadius, flags);

  // Render the badge text.
  canvas->DrawStringRect(GetText(), GetFont(),
                         color_provider->GetColor(kTextColor),
                         badge_text_bounds);
}

BEGIN_METADATA(BorealisBetaBadge)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Text)
END_METADATA

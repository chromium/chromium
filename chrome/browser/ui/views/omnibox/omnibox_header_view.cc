// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_header_view.h"

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

OmniboxHeaderView::OmniboxHeaderView(OmniboxPopupViewViews* popup_view)
    : popup_view_(popup_view) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  // This is the designer-provided spacing that matches the NTP Realbox.
  layout->set_between_child_spacing(0);

  header_label_ = AddChildView(std::make_unique<views::Label>());
  header_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  const gfx::FontList& font = views::TypographyProvider::Get().GetFont(
      CONTEXT_OMNIBOX_SECTION_HEADER, views::style::STYLE_PRIMARY);
  header_label_->SetFontList(font);
}

void OmniboxHeaderView::SetHeader(const std::u16string& header_text) {
  header_text_ = header_text;

  header_label_->SetText(header_text_);
}

gfx::Insets OmniboxHeaderView::GetInsets() const {
  // Makes the header height roughly the same as the single-line row height.
  constexpr int vertical = 8;

  // Aligns the header text with the icons of ordinary matches. The assumed
  // small icon width here is lame, but necessary, since it's not explicitly
  // defined anywhere else in the code.
  constexpr int assumed_match_cell_icon_width = 16;
  constexpr int left_inset = OmniboxMatchCellView::kMarginLeft +
                             (OmniboxMatchCellView::kImageBoundsWidth -
                              assumed_match_cell_icon_width) /
                                 2;

  return gfx::Insets::TLBR(vertical, left_inset, vertical,
                           OmniboxMatchCellView::kMarginRight);
}

bool OmniboxHeaderView::OnMousePressed(const ui::MouseEvent& event) {
  // Needed to ensure that clicking the header doesn't close the Omnibox.
  return true;
}

void OmniboxHeaderView::OnThemeChanged() {
  views::View::OnThemeChanged();
  header_label_->SetEnabledColor(
      GetColorProvider()->GetColor(kColorOmniboxResultsTextDimmed));
}

BEGIN_METADATA(OmniboxHeaderView)
END_METADATA

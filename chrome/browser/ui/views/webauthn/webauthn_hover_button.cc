// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/webauthn_hover_button.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/typography.h"

namespace {

// IconWrapper wraps an icon for placing it into a
// |WebAuthnHoverButton|. It is a near copy of the base HoverButton's
// internal class of the same name, but the horizontal spacing applied
// by that class is incompatible with the WebAuthn UI spec.
class IconWrapper : public views::View {
 public:
  METADATA_HEADER(IconWrapper);
  explicit IconWrapper(std::unique_ptr<views::View> icon)
      : icon_(AddChildView(std::move(icon))) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    // Make sure hovering over the icon also hovers the |HoverButton|.
    SetCanProcessEventsWithinSubtree(false);
    // Don't cover |icon| when the ink drops are being painted.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }

 private:
  views::View* icon_;
};

BEGIN_METADATA(IconWrapper, views::View)
END_METADATA

}  // namespace

WebAuthnHoverButton::WebAuthnHoverButton(
    PressedCallback callback,
    std::unique_ptr<views::ImageView> icon,
    const std::u16string& title_text,
    const std::u16string& subtitle_text,
    std::unique_ptr<views::View> secondary_icon,
    bool force_two_line)
    : HoverButton(std::move(callback), std::u16string()) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  views::GridLayout* grid_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  constexpr int kColumnSet = 0;
  views::ColumnSet* columns = grid_layout->AddColumnSet(kColumnSet);

  const int icon_padding = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  if (icon) {
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                       views::GridLayout::kFixedSize,
                       views::GridLayout::ColumnSize::kUsePreferred,
                       /*fixed_width=*/0,
                       /*min_width=*/0);
    columns->AddPaddingColumn(views::GridLayout::kFixedSize, icon_padding);
  }
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     /*resize_percent=*/1.0,
                     views::GridLayout::ColumnSize::kUsePreferred,
                     /*fixed_width=*/0,
                     /*min_width=*/0);
  if (secondary_icon) {
    columns->AddPaddingColumn(views::GridLayout::kFixedSize, icon_padding);
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                       views::GridLayout::kFixedSize,
                       views::GridLayout::ColumnSize::kUsePreferred,
                       /*fixed_width=*/0,
                       /*min_width=*/0);
  }

  const int row_height = views::style::GetLineHeight(
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
  grid_layout->StartRow(views::GridLayout::kFixedSize, kColumnSet, row_height);

  const bool is_two_line = !subtitle_text.empty() || force_two_line;
  const int icon_row_span = is_two_line ? 2 : 1;
  if (icon) {
    icon_view_ =
        grid_layout->AddView(std::make_unique<IconWrapper>(std::move(icon)),
                             /*col_span=*/1, icon_row_span);
  }

  const int title_row_span = force_two_line && subtitle_text.empty() ? 2 : 1;
  title_ = grid_layout->AddView(std::make_unique<views::Label>(title_text),
                                /*col_span=*/1, title_row_span);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (secondary_icon) {
    secondary_icon_view_ = grid_layout->AddView(
        std::make_unique<IconWrapper>(std::move(secondary_icon)),
        /*col_span=*/1, icon_row_span);
  }

  if (is_two_line) {
    grid_layout->StartRow(views::GridLayout::kFixedSize, kColumnSet,
                          row_height);
    if (icon_view_) {
      grid_layout->SkipColumns(1);
    }

    if (!subtitle_text.empty()) {
      subtitle_ =
          grid_layout->AddView(std::make_unique<views::Label>(subtitle_text));
      subtitle_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    } else {
      grid_layout->SkipColumns(1);
    }

    if (secondary_icon_view_) {
      grid_layout->SkipColumns(1);
    }
  }

  SetAccessibleName(subtitle_text.empty()
                        ? title_text
                        : base::JoinString({title_text, subtitle_text}, u"\n"));

  // Per WebAuthn UI specs, the top/bottom insets of hover buttons are 12dp for
  // a one-line button, and 8dp for a two-line button. Left/right insets are
  // 8dp assuming a 20dp primary icon, or no icon at all. (With a 24dp primary
  // icon, the left inset would be 12dp, but we don't currently have a button
  // with such an icon.)
  const int vert_inset = is_two_line ? 8 : 12;
  constexpr int horz_inset = 8;
  SetBorder(
      views::CreateEmptyBorder(vert_inset, horz_inset, vert_inset, horz_inset));

  Layout();
}

BEGIN_METADATA(WebAuthnHoverButton, HoverButton)
END_METADATA

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"

PageInfoHoverButton::PageInfoHoverButton(views::ButtonListener* listener,
                                         const gfx::ImageSkia& image_icon,
                                         int title_resource_id,
                                         const base::string16& secondary_text,
                                         int click_target_id,
                                         const base::string16& tooltip_text,
                                         const base::string16& subtitle_text)
    : HoverButton(listener, base::string16()) {
  label()->SetHandlesTooltips(false);
  auto icon = std::make_unique<NonAccessibleImageView>();
  icon->SetImage(image_icon);

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  views::GridLayout* grid_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  const int icon_label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  constexpr int kColumnSetId = 0;
  views::ColumnSet* columns = grid_layout->AddColumnSet(kColumnSetId);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);
  columns->AddPaddingColumn(views::GridLayout::kFixedSize, icon_label_spacing);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                     views::GridLayout::USE_PREF, 0, 0);

  // Make sure hovering over the icon also hovers the |PageInfoHoverButton|.
  icon->set_can_process_events_within_subtree(false);
  // Don't cover |icon_view| when the ink drops are being painted.
  icon->SetPaintToLayer();
  icon->layer()->SetFillsBoundsOpaquely(false);

  // Force row to have sufficient height for full line-height of the title.
  grid_layout->StartRow(
      views::GridLayout::kFixedSize, kColumnSetId,
      views::style::GetLineHeight(views::style::CONTEXT_LABEL,
                                  views::style::STYLE_PRIMARY));

  icon_view_ = grid_layout->AddView(std::move(icon));

  auto title_label =
      std::make_unique<views::StyledLabel>(base::string16(), nullptr);
  title_label->SetTextContext(views::style::CONTEXT_LABEL);
  // |views::StyledLabel|s are all multi-line. With a layout manager,
  // |StyledLabel| will try use the available space to size itself, and long
  // titles will wrap to the next line (for smaller |PageInfoHoverButton|s, this
  // will also cover up |subtitle_|). Wrap it in a parent view with no layout
  // manager to ensure it keeps its original size set by SizeToFit() above. Long
  // titles will then be truncated.
  auto title_wrapper = std::make_unique<views::View>();
  title_ = title_wrapper->AddChildView(std::move(title_label));
  SetTitleText(title_resource_id, secondary_text);

  // Hover the whole button when hovering |title_|. This is OK because |title_|
  // will never have a link in it.
  title_wrapper->set_can_process_events_within_subtree(false);
  grid_layout->AddView(std::move(title_wrapper));

  if (!subtitle_text.empty()) {
    grid_layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
    auto subtitle_label = std::make_unique<views::Label>(
        subtitle_text, views::style::CONTEXT_LABEL,
        views::style::STYLE_SECONDARY);
    subtitle_label->SetMultiLine(true);
    subtitle_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    subtitle_label->SetAutoColorReadabilityEnabled(false);
    grid_layout->SkipColumns(1);
    subtitle_ = grid_layout->AddView(std::move(subtitle_label));
  }

  auto insets = layout_provider->GetInsetsMetric(
      views::InsetsMetric::INSETS_LABEL_BUTTON);
  const int vert_spacing = insets.height();
  const int horz_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_HORIZONTAL_PADDING);
  SetBorder(views::CreateEmptyBorder(vert_spacing, horz_spacing, vert_spacing,
                                     horz_spacing));

  SetID(click_target_id);
  SetTooltipText(tooltip_text);
  UpdateAccessibleName();

  Layout();
}

void PageInfoHoverButton::SetTitleText(int title_resource_id,
                                       const base::string16& secondary_text) {
  DCHECK(title_);
  if (secondary_text.empty()) {
    title_->SetText(l10n_util::GetStringUTF16(title_resource_id));
  } else {
    size_t offset;
    auto title_text =
        l10n_util::GetStringFUTF16(title_resource_id, secondary_text, &offset);
    title_->SetText(title_text);
    views::StyledLabel::RangeStyleInfo style_info;
    style_info.text_style = views::style::STYLE_SECONDARY;
    title_->AddStyleRange(gfx::Range(offset, offset + secondary_text.length()),
                          style_info);
  }
  title_->SizeToFit(0);
  UpdateAccessibleName();
}

void PageInfoHoverButton::UpdateAccessibleName() {
  const base::string16 accessible_name =
      subtitle() == nullptr
          ? title()->GetText()
          : base::JoinString({title()->GetText(), subtitle()->GetText()},
                             base::ASCIIToUTF16("\n"));
  HoverButton::SetAccessibleName(accessible_name);
}

gfx::Size PageInfoHoverButton::CalculatePreferredSize() const {
  return Button::CalculatePreferredSize();
}

int PageInfoHoverButton::GetHeightForWidth(int w) const {
  return Button::GetHeightForWidth(w);
}

void PageInfoHoverButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  return Button::OnBoundsChanged(previous_bounds);
}

views::View* PageInfoHoverButton::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return Button::GetTooltipHandlerForPoint(point);
}

BEGIN_METADATA(PageInfoHoverButton)
METADATA_PARENT_CLASS(HoverButton)
END_METADATA()

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"

namespace {

std::unique_ptr<views::View> CreateIconView(const ui::ImageModel& icon_image) {
  auto icon = std::make_unique<NonAccessibleImageView>();
  icon->SetImage(icon_image);
  // Make sure hovering over the icon also hovers the `PageInfoHoverButton`.
  icon->SetCanProcessEventsWithinSubtree(false);
  // Don't cover |icon| when the ink drops are being painted.
  icon->SetPaintToLayer();
  icon->layer()->SetFillsBoundsOpaquely(false);
  return icon;
}

}  // namespace

PageInfoHoverButton::PageInfoHoverButton(
    views::Button::PressedCallback callback,
    const ui::ImageModel& main_image_icon,
    int title_resource_id,
    const std::u16string& secondary_text,
    int click_target_id,
    const std::u16string& tooltip_text,
    const std::u16string& subtitle_text,
    absl::optional<ui::ImageModel> action_image_icon)
    : HoverButton(std::move(callback), std::u16string()) {
  label()->SetHandlesTooltips(false);

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  views::GridLayout* grid_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  const int icon_label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  constexpr int kColumnSetId = 0;
  views::ColumnSet* columns = grid_layout->AddColumnSet(kColumnSetId);
  // TODO(olesiamarukhno): Unify the column width through all views in the
  // page info (PageInfoHoverButton, PermissionSelectorRow, ChosenObjectView,
  // SecurityInformationView). Currently, it isn't same everywhere and it
  // causes label text next to icon not to be aligned by 1 or 2px.
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  columns->AddPaddingColumn(views::GridLayout::kFixedSize, icon_label_spacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 1.0,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  columns->AddPaddingColumn(views::GridLayout::kFixedSize, icon_label_spacing);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kUsePreferred,
                     views::GridLayout::kFixedSize, 0);
  columns->AddPaddingColumn(views::GridLayout::kFixedSize, icon_label_spacing);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kFixed, 16, 0);

  views::style::TextContext text_context =
      views::style::CONTEXT_DIALOG_BODY_TEXT;

  // Force row to have sufficient height for full line-height of the title.
  grid_layout->StartRow(
      views::GridLayout::kFixedSize, kColumnSetId,
      views::style::GetLineHeight(text_context, views::style::STYLE_PRIMARY));

  grid_layout->AddView(CreateIconView(main_image_icon));
  auto title_label = std::make_unique<views::StyledLabel>();
  title_label->SetTextContext(text_context);

  title_ = grid_layout->AddView(std::move(title_label));
  title_->SetCanProcessEventsWithinSubtree(false);

  auto secondary_label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY);
  secondary_label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  secondary_label_ = grid_layout->AddView(std::move(secondary_label));

  if (action_image_icon.has_value()) {
    grid_layout->AddView(CreateIconView(action_image_icon.value()));
  }

  if (title_resource_id)
    SetTitleText(title_resource_id, secondary_text);

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

  SetBorder(views::CreateEmptyBorder(layout_provider->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON)));

  SetID(click_target_id);
  SetTooltipText(tooltip_text);
  UpdateAccessibleName();

  Layout();
}

void PageInfoHoverButton::SetTitleText(int title_resource_id,
                                       const std::u16string& secondary_text) {
  DCHECK(title_);
  title_->SetText(l10n_util::GetStringUTF16(title_resource_id));
  if (!secondary_text.empty()) {
    secondary_label_->SetText(secondary_text);
  }
  UpdateAccessibleName();
}

void PageInfoHoverButton::SetTitleText(const std::u16string& title_text) {
  DCHECK(title_);
  title_->SetText(title_text);
  UpdateAccessibleName();
}

void PageInfoHoverButton::SetSubtitleMultiline(bool is_multiline) {
  subtitle()->SetMultiLine(is_multiline);
}

void PageInfoHoverButton::UpdateAccessibleName() {
  const std::u16string title_text =
      secondary_label_ == nullptr
          ? title()->GetText()
          : base::JoinString({title()->GetText(), secondary_label_->GetText()},
                             u" ");
  const std::u16string accessible_name =
      subtitle() == nullptr
          ? title_text
          : base::JoinString({title_text, subtitle()->GetText()}, u"\n");
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

BEGIN_METADATA(PageInfoHoverButton, HoverButton)
END_METADATA

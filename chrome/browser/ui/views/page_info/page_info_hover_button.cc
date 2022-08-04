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
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

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
  const int icon_label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  views::style::TextContext text_context =
      views::style::CONTEXT_DIALOG_BODY_TEXT;
  // TODO(olesiamarukhno): Unify the column width through all views in the
  // page info (PageInfoHoverButton, PermissionSelectorRow, ChosenObjectView,
  // SecurityInformationView). Currently, it isn't same everywhere and it
  // causes label text next to icon not to be aligned by 1 or 2px.
  views::TableLayout* table_layout =
      SetLayoutManager(std::make_unique<views::TableLayout>());
  table_layout
      ->AddColumn(views::LayoutAlignment::kCenter,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kStretch,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kFixed, 16, 0)
      .AddRows(1, views::TableLayout::kFixedSize,
               // Force row to have sufficient height for full line-height of
               // the title.
               views::style::GetLineHeight(text_context,
                                           views::style::STYLE_PRIMARY));

  // TODO(pkasting): This class should subclass Button, not HoverButton.
  table_layout->SetChildViewIgnoredByLayout(image(), true);
  table_layout->SetChildViewIgnoredByLayout(label(), true);
  table_layout->SetChildViewIgnoredByLayout(ink_drop_container(), true);

  AddChildView(CreateIconView(main_image_icon));
  auto title_label = std::make_unique<views::StyledLabel>();
  title_label->SetTextContext(text_context);

  title_ = AddChildView(std::move(title_label));
  title_->SetCanProcessEventsWithinSubtree(false);

  auto secondary_label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY);
  secondary_label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  secondary_label_ = AddChildView(std::move(secondary_label));

  if (action_image_icon.has_value()) {
    AddChildView(CreateIconView(action_image_icon.value()));
  } else {
    // Fill the cell with an empty view at column 4.
    AddChildView(std::make_unique<views::View>());
  }

  if (title_resource_id)
    SetTitleText(title_resource_id, secondary_text);

  if (!subtitle_text.empty()) {
    table_layout->AddRows(1, views::TableLayout::kFixedSize);
    AddChildView(std::make_unique<views::View>());
    subtitle_ = AddChildView(std::make_unique<views::Label>(
        subtitle_text, views::style::CONTEXT_LABEL,
        views::style::STYLE_SECONDARY));
    subtitle_->SetMultiLine(true);
    subtitle_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    subtitle_->SetAutoColorReadabilityEnabled(false);
    AddChildView(std::make_unique<views::View>());
    AddChildView(std::make_unique<views::View>());
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

void PageInfoHoverButton::SetSubtitleText(const std::u16string& subtitle_text) {
  DCHECK(subtitle_);
  subtitle_->SetText(subtitle_text);
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

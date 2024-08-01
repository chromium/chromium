// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/rich_hover_button.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<views::View> CreateIconView(const ui::ImageModel& icon_image) {
  auto icon = std::make_unique<NonAccessibleImageView>();
  icon->SetImage(icon_image);
  // Make sure hovering over the icon also hovers the `RichHoverButton`.
  icon->SetCanProcessEventsWithinSubtree(false);
  // Don't cover |icon| when the ink drops are being painted.
  icon->SetPaintToLayer();
  icon->layer()->SetFillsBoundsOpaquely(false);
  return icon;
}

// TODO(crbug.com/355018927): Remove this when we implement in views::Label.
class SubtitleLabelWrapper : public views::View {
  METADATA_HEADER(SubtitleLabelWrapper, views::View)
 public:
  explicit SubtitleLabelWrapper(std::unique_ptr<views::View> title) {
    SetUseDefaultFillLayout(true);
    title_ = AddChildView(std::move(title));
  }

 private:
  // View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size preferred_size = title_->GetPreferredSize(available_size);
    if (!available_size.width().is_bounded()) {
      preferred_size.set_width(title_->GetMinimumSize().width());
    }
    return preferred_size;
  }

  raw_ptr<views::View> title_ = nullptr;
};

BEGIN_METADATA(SubtitleLabelWrapper)
END_METADATA

}  // namespace

RichHoverButton::RichHoverButton(
    views::Button::PressedCallback callback,
    const ui::ImageModel& main_image_icon,
    const std::u16string& title_text,
    const std::u16string& secondary_text,
    const std::u16string& tooltip_text,
    const std::u16string& subtitle_text,
    std::optional<ui::ImageModel> action_image_icon,
    std::optional<ui::ImageModel> state_icon)
    : HoverButton(std::move(callback), std::u16string()) {
  label()->SetHandlesTooltips(false);

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int icon_label_spacing = layout_provider->GetDistanceMetric(
      DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL);
  views::style::TextContext text_context =
      views::style::CONTEXT_DIALOG_BODY_TEXT;

  views::TableLayout* table_layout =
      SetLayoutManager(std::make_unique<views::TableLayout>());
  table_layout
      // Column for |main_image_icon|.
      ->AddColumn(views::LayoutAlignment::kCenter,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      // Column for title.
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      // Column for |secondary_text|.
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kStretch,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  if (state_icon.has_value()) {
    table_layout
        // Column for |state_icon|.
        ->AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
        .AddColumn(views::LayoutAlignment::kCenter,
                   views::LayoutAlignment::kCenter,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kFixed, 16, 0);
  }
  table_layout
      // Column for |action_icon|.
      ->AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kFixed, 16, 0)
      .AddRows(1, views::TableLayout::kFixedSize,
               // Force row to have sufficient height for full line-height of
               // the title.
               views::TypographyProvider::Get().GetLineHeight(
                   text_context, views::style::STYLE_PRIMARY));

  // TODO(pkasting): This class should subclass Button, not HoverButton.
  image_container_view()->SetProperty(views::kViewIgnoredByLayoutKey, true);
  label()->SetProperty(views::kViewIgnoredByLayoutKey, true);
  ink_drop_container()->SetProperty(views::kViewIgnoredByLayoutKey, true);

  AddChildView(CreateIconView(main_image_icon));
  auto title_label = std::make_unique<views::Label>();
  title_label->SetTextContext(text_context);

  title_ = AddChildView(std::move(title_label));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetCanProcessEventsWithinSubtree(false);

  auto secondary_label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY);
  secondary_label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  secondary_label_ = AddChildView(std::move(secondary_label));

  title_->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  secondary_label_->SetTextStyle(views::style::STYLE_BODY_5);
  secondary_label_->SetEnabledColorId(ui::kColorLabelForegroundSecondary);

  // State icon is optional and column is created only when it is set.
  if (state_icon.has_value()) {
    AddChildView(CreateIconView(state_icon.value()));
  }

  if (action_image_icon.has_value()) {
    AddChildView(CreateIconView(action_image_icon.value()));
  } else {
    // Fill the cell with an empty view at column 5.
    AddChildView(std::make_unique<views::View>());
  }

  if (!title_text.empty()) {
    SetTitleText(title_text);
    if (!secondary_text.empty())
      SetSecondaryText(secondary_text);
  }

  if (!subtitle_text.empty()) {
    table_layout->AddRows(1, views::TableLayout::kFixedSize);
    AddChildView(std::make_unique<views::View>());
    auto subtitle = std::make_unique<views::Label>(
        subtitle_text, views::style::CONTEXT_LABEL,
        views::style::STYLE_SECONDARY);
    subtitle_ = subtitle.get();

    AddChildView(std::make_unique<SubtitleLabelWrapper>(std::move(subtitle)));
    subtitle_->SetTextStyle(views::style::STYLE_BODY_5);
    subtitle_->SetEnabledColorId(ui::kColorLabelForegroundSecondary);
    subtitle_->SetMultiLine(true);
    subtitle_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    subtitle_->SetAutoColorReadabilityEnabled(false);
    AddChildView(std::make_unique<views::View>());
    AddChildView(std::make_unique<views::View>());
  }

  SetBorder(views::CreateEmptyBorder(layout_provider->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON)));

  SetTooltipText(tooltip_text);
  UpdateAccessibleName();

  DeprecatedLayoutImmediately();
}

void RichHoverButton::SetTitleText(const std::u16string& title_text) {
  DCHECK(title_);
  title_->SetText(title_text);
  UpdateAccessibleName();
}

void RichHoverButton::SetSecondaryText(const std::u16string& secondary_text) {
  DCHECK(secondary_label_);
  secondary_label_->SetText(secondary_text);
  UpdateAccessibleName();
}

void RichHoverButton::SetSubtitleText(const std::u16string& subtitle_text) {
  DCHECK(subtitle_);
  subtitle_->SetText(subtitle_text);
  UpdateAccessibleName();
}

void RichHoverButton::SetSubtitleMultiline(bool is_multiline) {
  subtitle_->SetMultiLine(is_multiline);
}

const views::Label* RichHoverButton::GetTitleViewForTesting() const {
  return title_;
}

const views::Label* RichHoverButton::GetSubTitleViewForTesting() const {
  return subtitle_;
}

void RichHoverButton::UpdateAccessibleName() {
  const std::u16string title_text =
      secondary_label_ == nullptr
          ? title_->GetText()
          : base::JoinString({title_->GetText(), secondary_label_->GetText()},
                             u" ");
  const std::u16string accessible_name =
      subtitle_ == nullptr
          ? title_text
          : base::JoinString({title_text, subtitle_->GetText()}, u"\n");
  HoverButton::GetViewAccessibility().SetName(accessible_name);
}

gfx::Size RichHoverButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return Button::CalculatePreferredSize(available_size);
}

void RichHoverButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  return Button::OnBoundsChanged(previous_bounds);
}

views::View* RichHoverButton::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return Button::GetTooltipHandlerForPoint(point);
}

BEGIN_METADATA(RichHoverButton)
END_METADATA

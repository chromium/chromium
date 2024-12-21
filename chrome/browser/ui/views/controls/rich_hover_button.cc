// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/rich_hover_button.h"

#include <string>

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

std::unique_ptr<views::ImageView> CreateIconView(ui::ImageModel icon) {
  auto view = std::make_unique<NonAccessibleImageView>();
  view->SetImage(std::move(icon));
  // Make sure hovering over the icon also hovers the `RichHoverButton`.
  view->SetCanProcessEventsWithinSubtree(false);
  // Don't cover |icon| when the ink drops are being painted.
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
  return view;
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

RichHoverButton::RichHoverButton(views::Button::PressedCallback callback,
                                 ui::ImageModel icon,
                                 const std::u16string& title_text,
                                 const std::u16string& subtitle_text,
                                 ui::ImageModel action_icon,
                                 ui::ImageModel state_icon) {
  image_container_view()->SetProperty(views::kViewIgnoredByLayoutKey, true);
  label()->SetHandlesTooltips(false);
  label()->SetProperty(views::kViewIgnoredByLayoutKey, true);
  ink_drop_container()->SetProperty(views::kViewIgnoredByLayoutKey, true);

  start_ = children().size();

  SetBorder(
      views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
          ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON)));
  // Main icon placeholder.
  AddChildView(std::make_unique<views::View>());
  // Title.
  title_ = AddChildView(std::make_unique<views::Label>());
  title_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  title_->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetCanProcessEventsWithinSubtree(false);
  // Action icon placeholder.
  AddChildView(std::make_unique<views::View>());

  custom_view_row_start_ = children().size();

  RecreateLayout();

  SetCallback(std::move(callback));
  SetIcon(std::move(icon));
  SetTitleText(title_text);
  SetStateIcon(std::move(state_icon));
  SetActionIcon(std::move(action_icon));
  SetSubtitleText(subtitle_text);
}

RichHoverButton::~RichHoverButton() = default;

void RichHoverButton::SetIcon(ui::ImageModel icon) {
  SetIconMember(icon_, start_, std::move(icon), true);
}

void RichHoverButton::SetTitleText(const std::u16string& title_text) {
  title_->SetText(title_text);
  UpdateAccessibleName();
}

void RichHoverButton::SetStateIcon(ui::ImageModel state_icon) {
  SetIconMember(state_icon_, start_ + 2, std::move(state_icon), false);
}

void RichHoverButton::SetActionIcon(ui::ImageModel action_icon) {
  SetIconMember(action_icon_, start_ + (state_icon_ ? 3 : 2),
                std::move(action_icon), true);
}

void RichHoverButton::SetSubtitleText(const std::u16string& subtitle_text) {
  if (subtitle_text.empty()) {
    subtitle_ = nullptr;
    for (const auto& v : subtitle_row_views_) {
      RemoveChildViewT(v);
    }
    subtitle_row_views_.clear();
  } else {
    if (subtitle_row_views_.empty()) {
      subtitle_row_views_.push_back(AddChildView(
          std::make_unique<views::View>()));  // Skip main icon column.
      auto subtitle = std::make_unique<views::Label>();
      subtitle_ = subtitle.get();
      subtitle_row_views_.push_back(AddChildView(
          std::make_unique<SubtitleLabelWrapper>(std::move(subtitle))));
      subtitle_->SetTextStyle(views::style::STYLE_BODY_5);
      subtitle_->SetEnabledColorId(ui::kColorLabelForegroundSecondary);
      subtitle_->SetMultiLine(true);
      subtitle_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      subtitle_->SetAutoColorReadabilityEnabled(false);
      base::Extend(subtitle_row_views_, AddFillerViews(children().size()));
    }
    subtitle_->SetText(subtitle_text);
  }
  RecreateLayout();
  UpdateAccessibleName();
}

void RichHoverButton::SetSubtitleMultiline(bool is_multiline) {
  subtitle_->SetMultiLine(is_multiline);
}

void RichHoverButton::SetTitleTextStyleAndColor(int style,
                                                ui::ColorId color_id) {
  title_->SetTextStyle(style);
  title_->SetEnabledColorId(color_id);
}

void RichHoverButton::SetSubtitleTextStyleAndColor(int style,
                                                   ui::ColorId color_id) {
  if (subtitle_) {
    subtitle_->SetTextStyle(style);
    subtitle_->SetEnabledColorId(color_id);
  }
}

const views::Label* RichHoverButton::GetTitleViewForTesting() const {
  return title_;
}

const views::Label* RichHoverButton::GetSubTitleViewForTesting() const {
  return subtitle_;
}

void RichHoverButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  return Button::OnBoundsChanged(previous_bounds);
}

views::View* RichHoverButton::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return Button::GetTooltipHandlerForPoint(point);
}

gfx::Size RichHoverButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return Button::CalculatePreferredSize(available_size);
}

void RichHoverButton::SetIconMember(raw_ptr<views::ImageView>& icon_member,
                                    size_t child_index,
                                    ui::ImageModel icon,
                                    bool use_placeholder) {
  if (icon.IsEmpty()) {
    if (icon_member) {
      icon_member = nullptr;
      RemoveChildViewT(children()[child_index]);
      if (use_placeholder) {
        AddChildViewAt(std::make_unique<views::View>(), child_index);
      } else {
        RecreateLayout();
      }
    }
  } else if (!icon_member) {
    if (use_placeholder) {
      RemoveChildViewT(children()[child_index]);
    }
    icon_member = AddChildViewAt(CreateIconView(std::move(icon)), child_index);
    if (!use_placeholder) {
      RecreateLayout();
    }
  } else {
    icon_member->SetImage(std::move(icon));
  }
}

void RichHoverButton::RecreateLayout() {
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL);
  views::TableLayout* table_layout =
      SetLayoutManager(std::make_unique<views::TableLayout>());
  table_layout
      // Column for main image.
      ->AddColumn(views::LayoutAlignment::kCenter,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      // Column for title.
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  if (state_icon_) {
    table_layout
        // Column for state icon.
        ->AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
        .AddColumn(views::LayoutAlignment::kCenter,
                   views::LayoutAlignment::kCenter,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kFixed, 16, 0);
  }
  table_layout
      // Column for action icon.
      ->AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kFixed, 16, 0)
      .AddRows(1, views::TableLayout::kFixedSize,
               // Force row to have sufficient height for full line-height of
               // the title.
               views::TypographyProvider::Get().GetLineHeight(
                   views::style::CONTEXT_DIALOG_BODY_TEXT,
                   views::style::STYLE_PRIMARY));
  if (!custom_view_row_views_.empty()) {
    // Row for custom view.
    table_layout->AddRows(1, views::TableLayout::kFixedSize);
  }
  if (!subtitle_row_views_.empty()) {
    // Row for subtitle.
    table_layout->AddRows(1, views::TableLayout::kFixedSize);
  }
}

void RichHoverButton::UpdateAccessibleName() {
  const std::u16string title_text = title_->GetText();
  const std::u16string accessible_name =
      subtitle_ == nullptr
          ? title_text
          : base::JoinString({title_text, subtitle_->GetText()}, u"\n");
  HoverButton::GetViewAccessibility().SetName(accessible_name);
}

std::vector<raw_ptr<views::View>> RichHoverButton::AddFillerViews(
    size_t start) {
  std::vector<raw_ptr<views::View>> vec;
  if (state_icon_) {
    vec.push_back(AddChildViewAt(std::make_unique<views::View>(), start++));
  }
  vec.push_back(AddChildViewAt(std::make_unique<views::View>(), start));
  return vec;
}

BEGIN_METADATA(RichHoverButton)
END_METADATA

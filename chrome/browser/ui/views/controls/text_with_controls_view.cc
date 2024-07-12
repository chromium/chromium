// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/text_with_controls_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace {
std::unique_ptr<views::View> CreateLabelWrapper() {
  auto label_wrapper = std::make_unique<views::BoxLayoutView>();
  label_wrapper->SetOrientation(views::LayoutOrientation::kVertical);
  label_wrapper->SetProperty(views::kMarginsKey, gfx::Insets::VH(0, 0));
  label_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1));
  return label_wrapper;
}
}  // namespace

TextWithControlsView::TextWithControlsView() {
  SetLayoutManagerUseConstrainedSpace(true);
  auto button_insets = ChromeLayoutProvider::Get()->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);
  SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  SetInteriorMargin(button_insets);

  labels_wrapper_ = AddChildView(CreateLabelWrapper());
  title_ = labels_wrapper_->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT));
  title_->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  const int toggle_size = GetLayoutConstant(PAGE_INFO_ICON_SIZE);

  // Calculate difference between label height and icon size to align the toggle
  // to the top of the title.
  const int label_height =
      title_->GetPreferredSize(views::SizeBounds(title_->width(), {})).height();
  const int margin = (label_height - toggle_size) / 2;
  SetDefault(views::kMarginsKey, gfx::Insets::VH(margin, 0));

  description_ = AddSecondaryLabel(u"");
}

void TextWithControlsView::SetTitle(std::u16string title) {
  title_->SetText(title);
}

void TextWithControlsView::SetDescription(std::u16string description) {
  description_->SetText(description);
}

int TextWithControlsView::GetFirstLineHeight() {
  return title_->GetLineHeight();
}

void TextWithControlsView::SetVisible(bool visible) {
  views::View::SetVisible(visible);
  PreferredSizeChanged();
}

views::Label* TextWithControlsView::AddSecondaryLabel(std::u16string text) {
  auto secondary_label = std::make_unique<views::Label>(
      text, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
  secondary_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  secondary_label->SetMultiLine(true);
  secondary_label->SetProperty(
      views::kBoxLayoutFlexKey,
      views::BoxLayoutFlexSpecification().WithWeight(1));
  secondary_label->SetTextStyle(views::style::STYLE_BODY_5);
  return labels_wrapper_->AddChildView(std::move(secondary_label));
}

gfx::Size TextWithControlsView::FlexRule(
    const views::View* view,
    const views::SizeBounds& maximum_size) const {
  return CalculatePreferredSize(maximum_size);
}

gfx::Size TextWithControlsView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (available_size.width().is_bounded()) {
    return FlexLayoutView::CalculatePreferredSize(available_size);
  }

  // Secondary labels can be multiline. To wrap them properly, calculate here
  // the width of the row without them. This way, if a secondary label is too
  // long, it won't expand the width of the row.
  int width = GetInteriorMargin().width();
  width += labels_wrapper_->GetProperty(views::kMarginsKey)->width();
  width += title_->GetPreferredSize().width();
  width += controls_width_;

  width = std::max(width, ChromeLayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  return gfx::Size(width,
                   GetLayoutManager()->GetPreferredHeightForWidth(this, width));
}

BEGIN_METADATA(TextWithControlsView)
END_METADATA

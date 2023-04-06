// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_row_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

PageInfoRowView::PageInfoRowView() {
  auto button_insets = ChromeLayoutProvider::Get()->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);
  layout_manager_ = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_->SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetInteriorMargin(button_insets);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  const int icon_size = GetLayoutConstant(PAGE_INFO_ICON_SIZE);
  icon_->SetImageSize({icon_size, icon_size});

  labels_wrapper_ = AddChildView(PageInfoViewFactory::CreateLabelWrapper());
  title_ = labels_wrapper_->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Calculate difference between label height and icon size to align icons
  // and label in the first row.
  const int label_height = title_->GetPreferredSize().height();
  const int margin = (label_height - icon_size) / 2;
  layout_manager_->SetDefault(views::kMarginsKey, gfx::Insets::VH(margin, 0));
}

void PageInfoRowView::SetIcon(const ui::ImageModel image) {
  icon_->SetImage(image);
}

void PageInfoRowView::SetTitle(std::u16string title) {
  title_->SetText(title);
}

int PageInfoRowView::GetFirstLineHeight() {
  return title_->GetLineHeight();
}

views::Label* PageInfoRowView::AddSecondaryLabel(std::u16string text) {
  auto secondary_label = std::make_unique<views::Label>(
      text, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
  secondary_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  secondary_label->SetMultiLine(true);
  secondary_label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1));
  secondary_label->SetProperty(views::kCrossAxisAlignmentKey,
                               views::LayoutAlignment::kStart);
  return labels_wrapper_->AddChildView(std::move(secondary_label));
}

gfx::Size PageInfoRowView::FlexRule(
    const views::View* view,
    const views::SizeBounds& maximum_size) const {
  gfx::Size preferred_size = GetPreferredSize();
  if (!maximum_size.width().is_bounded()) {
    return preferred_size;
  }

  const int maximum_width = maximum_size.width().value();
  // If content view contains scrollbar, update the preferred width to account
  // for it.
  const int scrollbar_width =
      std::max(PageInfoViewFactory::kMinBubbleWidth - maximum_width, 0);
  const int width =
      std::max(preferred_size.width() - scrollbar_width, maximum_width);
  return gfx::Size(width, views::View::GetHeightForWidth(width));
}

gfx::Size PageInfoRowView::CalculatePreferredSize() const {
  // Secondary labels can be multiline. To wrap them properly, calculate here
  // the width of the row without them. This way, if a secondary label is too
  // long, it won't expand the width of the row.
  int width = layout_manager_->interior_margin().width();
  width += labels_wrapper_->GetProperty(views::kMarginsKey)->width();
  width += icon_->GetPreferredSize().width();
  width += title_->GetPreferredSize().width();
  width += controls_width_;

  width = std::max(width, PageInfoViewFactory::kMinBubbleWidth);
  return gfx::Size(width, views::View::GetHeightForWidth(width));
}

const std::u16string& PageInfoRowView::GetTitleForTesting() {
  return title_->GetText();
}

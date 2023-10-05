// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace {
// TODO(crbug.com/1446230): Consider moving this method to a Factory class
// and refactor PageInfoViewFactory::CreateLabelWrapper.
std::unique_ptr<views::View> CreateLabelWrapper() {
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL);
  auto label_wrapper = std::make_unique<views::FlexLayoutView>();
  label_wrapper->SetOrientation(views::LayoutOrientation::kVertical);
  label_wrapper->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(0, icon_label_spacing));
  label_wrapper->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kStretch);
  label_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1));
  return label_wrapper;
}
}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(RichControlsContainerView, kIcon);

RichControlsContainerView::RichControlsContainerView() {
  auto button_insets = ChromeLayoutProvider::Get()->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);
  SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  SetInteriorMargin(button_insets);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  const int icon_size = GetLayoutConstant(PAGE_INFO_ICON_SIZE);
  icon_->SetImageSize({icon_size, icon_size});
  icon_->SetProperty(views::kElementIdentifierKey, kIcon);

  labels_wrapper_ = AddChildView(CreateLabelWrapper());
  title_ = labels_wrapper_->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT));
  if (features::IsChromeRefresh2023()) {
    title_->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  }
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Calculate difference between label height and icon size to align icons
  // and label in the first row.
  // TODO(crbug.com/1446230): Refactor the view and use a TableLayout instead.
  const int label_height = title_->GetPreferredSize().height();
  const int margin = (label_height - icon_size) / 2;
  SetDefault(views::kMarginsKey, gfx::Insets::VH(margin, 0));
}

void RichControlsContainerView::SetIcon(const ui::ImageModel image) {
  icon_->SetImage(image);
}

void RichControlsContainerView::SetTitle(std::u16string title) {
  title_->SetText(title);
}

int RichControlsContainerView::GetFirstLineHeight() {
  return title_->GetLineHeight();
}

views::Label* RichControlsContainerView::AddSecondaryLabel(
    std::u16string text) {
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

gfx::Size RichControlsContainerView::FlexRule(
    const views::View* view,
    const views::SizeBounds& maximum_size) const {
  gfx::Size preferred_size = GetPreferredSize();
  if (!maximum_size.width().is_bounded()) {
    return preferred_size;
  }

  const int maximum_width = maximum_size.width().value();
  // If content view contains scrollbar, update the preferred width to account
  // for it.
  const int scrollbar_width = std::max(GetMinBubbleWidth() - maximum_width, 0);
  const int width =
      std::max(preferred_size.width() - scrollbar_width, maximum_width);
  return gfx::Size(width, views::View::GetHeightForWidth(width));
}

gfx::Size RichControlsContainerView::CalculatePreferredSize() const {
  // Secondary labels can be multiline. To wrap them properly, calculate here
  // the width of the row without them. This way, if a secondary label is too
  // long, it won't expand the width of the row.
  int width = GetInteriorMargin().width();
  width += labels_wrapper_->GetProperty(views::kMarginsKey)->width();
  width += icon_->GetPreferredSize().width();
  width += title_->GetPreferredSize().width();
  width += controls_width_;

  width = std::max(width, GetMinBubbleWidth());
  return gfx::Size(width, views::View::GetHeightForWidth(width));
}

const std::u16string& RichControlsContainerView::GetTitleForTesting() {
  return title_->GetText();
}

int RichControlsContainerView::GetMinBubbleWidth() const {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
}

BEGIN_METADATA(RichControlsContainerView, views::FlexLayoutView)
END_METADATA

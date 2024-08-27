// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_view.h"

#include <climits>
#include <memory>

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_delegate.h"

namespace toasts {
ToastView::ToastView(views::View* anchor_view,
                     const std::u16string& toast_text,
                     const gfx::VectorIcon& icon)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::NONE),
      toast_text_(toast_text),
      icon_(icon) {
  SetShowCloseButton(false);
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_corner_radius(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TOAST_BUBBLE_HEIGHT));
  SetProperty(views::kElementIdentifierKey, kToastElementId);
  set_close_on_deactivate(false);
}

ToastView::~ToastView() = default;

void ToastView::Init() {
  ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets()))
      ->set_between_child_spacing(
          lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING));

  icon_view_ = AddChildView(std::make_unique<views::ImageView>());

  label_ = AddChildView(std::make_unique<views::Label>(
      toast_text_, views::style::CONTEXT_BUTTON, views::style::STYLE_PRIMARY));
  label_->SetMultiLine(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetAllowCharacterBreak(false);
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetLineHeight(
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT));

  // Height of the toast is set implicitly by adding margins depending on the
  // height of the tallest child.
  int total_vertical_margins =
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT) -
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT);
  int top_margin = total_vertical_margins / 2;
  set_margins(gfx::Insets::TLBR(
      top_margin, lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_LEFT),
      total_vertical_margins - top_margin,
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_LABEL)));
}

gfx::Rect ToastView::GetBubbleBounds() {
  views::View* anchor_view = GetAnchorView();
  if (!anchor_view) {
    return gfx::Rect();
  }

  const gfx::Size bubble_size =
      GetWidget()->GetContentsView()->GetPreferredSize();
  const gfx::Rect anchor_bounds = anchor_view->GetBoundsInScreen();
  const int x =
      anchor_bounds.x() + (anchor_bounds.width() - bubble_size.width()) / 2;
  // Take bubble out of its original bounds to cross "line of death".
  const int y = anchor_bounds.bottom() - bubble_size.height() / 2;
  return gfx::Rect(x, y, bubble_size.width(), bubble_size.height());
}

void ToastView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  set_color(color_provider->GetColor(ui::kColorToastBackground));
  icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
      *icon_, color_provider->GetColor(ui::kColorToastForeground),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT)));
  label_->SetEnabledColor(color_provider->GetColor(ui::kColorToastForeground));
}

std::u16string ToastView::GetAccessibleWindowTitle() const {
  return toast_text_;
}

BEGIN_METADATA(ToastView)
END_METADATA

}  // namespace toasts

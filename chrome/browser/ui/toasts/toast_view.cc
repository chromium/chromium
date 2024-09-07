// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_view.h"

#include <climits>
#include <memory>

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace toasts {
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToastView, kToastViewId);

ToastView::ToastView(views::View* anchor_view,
                     const std::u16string& toast_text,
                     const gfx::VectorIcon& icon,
                     bool has_close_button)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::NONE),
      toast_text_(toast_text),
      icon_(icon),
      has_close_button_(has_close_button) {
  SetShowCloseButton(false);
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_corner_radius(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TOAST_BUBBLE_HEIGHT));
  SetProperty(views::kElementIdentifierKey, kToastElementId);
  set_close_on_deactivate(false);
  SetProperty(views::kElementIdentifierKey, kToastViewId);
}

ToastView::~ToastView() = default;

void ToastView::AddActionButton(const std::u16string& action_button_text,
                                base::RepeatingClosure action_button_callback) {
  has_action_button_ = true;
  action_button_text_ = action_button_text;
  action_button_callback_ = std::move(action_button_callback);
}

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
  label_->SetEnabledColorId(ui::kColorToastForeground);
  label_->SetMultiLine(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetAllowCharacterBreak(false);
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetLineHeight(
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT));

  if (has_action_button_) {
    label_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(
            0, 0, 0,
            lp->GetDistanceMetric(
                DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_ACTION_BUTTON_SPACING) -
                lp->GetDistanceMetric(
                    DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING)));

    action_button_ = AddChildView(std::make_unique<views::MdTextButton>(
        action_button_callback_.Then(
            base::BindRepeating(&ToastView::Close, base::Unretained(this),
                                ToastCloseReason::kActionButton)),
        action_button_text_));
    action_button_->SetEnabledTextColorIds(ui::kColorToastButton);
    action_button_->SetBgColorIdOverride(ui::kColorToastBackground);
    action_button_->SetStrokeColorIdOverride(ui::kColorToastButton);
    action_button_->SetPreferredSize(gfx::Size(
        action_button_->GetPreferredSize().width(),
        lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON)));
    action_button_->SetStyle(ui::ButtonStyle::kProminent);
    action_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
  }

  if (has_close_button_) {
    close_button_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
        base::BindRepeating(&ToastView::Close, base::Unretained(this),
                            ToastCloseReason::kCloseButton),
        vector_icons::kCloseIcon,
        lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT) -
            lp->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON).height(),
        ui::kColorToastForeground));
    views::InstallCircleHighlightPathGenerator(close_button_);
    close_button_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_CLOSE));
  }

  // Height of the toast is set implicitly by adding margins depending on the
  // height of the tallest child.
  const int total_vertical_margins =
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT) -
      lp->GetDistanceMetric(action_button_
                                ? DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON
                                : DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT);
  const int top_margin = total_vertical_margins / 2;
  const int right_margin = lp->GetDistanceMetric(
      close_button_    ? DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_CLOSE_BUTTON
      : action_button_ ? DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_ACTION_BUTTON
                       : DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_LABEL);
  set_margins(gfx::Insets::TLBR(
      top_margin, lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_LEFT),
      total_vertical_margins - top_margin, right_margin));
}

void ToastView::Close(ToastCloseReason reason) {
  // TODO(crbug.com/358610872): Log toast close reason metric.
  views::Widget::ClosedReason widget_closed_reason =
      views::Widget::ClosedReason::kUnspecified;
  switch (reason) {
    case ToastCloseReason::kCloseButton:
      widget_closed_reason = views::Widget::ClosedReason::kCloseButtonClicked;
      break;
    case ToastCloseReason::kActionButton:
      widget_closed_reason = views::Widget::ClosedReason::kAcceptButtonClicked;
      break;
    default:
      break;
  }
  // TODO(crbug.com/358615317): Make the toast animate out.
  GetWidget()->CloseWithReason(widget_closed_reason);
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
}

std::u16string ToastView::GetAccessibleWindowTitle() const {
  return toast_text_;
}

BEGIN_METADATA(ToastView)
END_METADATA

}  // namespace toasts

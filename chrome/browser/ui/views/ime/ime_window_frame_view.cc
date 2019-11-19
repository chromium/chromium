// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime/ime_window_frame_view.h"

#include "chrome/browser/ui/views/ime/ime_window_view.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ui {

ImeWindowFrameView::ImeWindowFrameView(ImeWindowView* ime_window_view,
                                       ImeWindow::Mode mode)
    : ime_window_view_(ime_window_view), mode_(mode) {}

ImeWindowFrameView::~ImeWindowFrameView() = default;

void ImeWindowFrameView::Init() {
  auto* outer_layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  auto* titlebar = AddChildView(std::make_unique<views::View>());
  auto* titlebar_layout =
      titlebar->SetLayoutManager(std::make_unique<views::FlexLayout>());
  titlebar_layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  auto content = std::make_unique<views::View>();
  content->SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification::ForSizeRule(
                           views::MinimumFlexSizeRule::kPreferred,
                           views::MaximumFlexSizeRule::kUnbounded));

  constexpr int kLeadingMarginDp = 8, kTrailingMarginDp = 6;
  if (in_follow_cursor_mode()) {
    titlebar->SetPreferredSize({kTitlebarHeight, 0});
    titlebar_layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetInteriorMargin({kLeadingMarginDp, 0, kTrailingMarginDp, 0});
    content->SetBorder(views::CreateEmptyBorder(
        {kImeBorderThickness, 0, kImeBorderThickness, kImeBorderThickness}));
  } else {
    outer_layout->SetOrientation(views::LayoutOrientation::kVertical);
    titlebar->SetPreferredSize({0, kTitlebarHeight});
    titlebar_layout->SetInteriorMargin(
        {0, kLeadingMarginDp, 0, kTrailingMarginDp});
    content->SetBorder(views::CreateEmptyBorder(
        {0, kImeBorderThickness, kImeBorderThickness, kImeBorderThickness}));
  }

  content_ = AddChildView(std::move(content));

  auto title_icon = std::make_unique<views::ImageView>();
  title_icon->SetImage(ime_window_view_->GetWindowIcon());
  title_icon->SetImageSize({16, 16});
  title_icon->set_tooltip_text(ime_window_view_->GetWindowTitle());
  title_icon->SetProperty(views::kFlexBehaviorKey,
                          views::FlexSpecification::ForSizeRule(
                              views::MinimumFlexSizeRule::kPreferred,
                              views::MaximumFlexSizeRule::kUnbounded)
                              .WithAlignment(views::LayoutAlignment::kStart));
  title_icon_ = titlebar->AddChildView(std::move(title_icon));

  if (!in_follow_cursor_mode()) {
    auto close_button = std::make_unique<views::ImageButton>(this);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    close_button->SetImage(views::Button::STATE_NORMAL,
                           rb.GetImageSkiaNamed(IDR_IME_WINDOW_CLOSE));
    close_button->SetImage(views::Button::STATE_HOVERED,
                           rb.GetImageSkiaNamed(IDR_IME_WINDOW_CLOSE_H));
    close_button->SetImage(views::Button::STATE_PRESSED,
                           rb.GetImageSkiaNamed(IDR_IME_WINDOW_CLOSE_C));
    close_button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    close_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    close_button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
    close_button->SetProperty(views::kFlexBehaviorKey,
                              views::FlexSpecification::ForSizeRule(
                                  views::MinimumFlexSizeRule::kPreferred,
                                  views::MaximumFlexSizeRule::kUnbounded)
                                  .WithAlignment(views::LayoutAlignment::kEnd));
    close_button_ = titlebar->AddChildView(std::move(close_button));
  }
}

void ImeWindowFrameView::UpdateIcon() {
  UpdateWindowIcon();
}

gfx::Rect ImeWindowFrameView::GetBoundsForClientView() const {
  return content_->ConvertRectToParent(content_->GetContentsBounds());
}

gfx::Rect ImeWindowFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  // The window bounds include both client area and non-client area (titlebar
  // and left, right and bottom borders).
  if (in_follow_cursor_mode()) {
    return gfx::Rect(
        client_bounds.x() - kTitlebarHeight,
        client_bounds.y() - kImeBorderThickness,
        client_bounds.width() + kTitlebarHeight + kImeBorderThickness,
        client_bounds.height() + kImeBorderThickness * 2);
  }
  return gfx::Rect(
      client_bounds.x() - kImeBorderThickness,
      client_bounds.y() - kTitlebarHeight,
      client_bounds.width() + kImeBorderThickness * 2,
      client_bounds.height() + kTitlebarHeight + kImeBorderThickness);
}

int ImeWindowFrameView::NonClientHitTest(const gfx::Point& point) {
  int client_component =
      ime_window_view_->window()->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE)
    return client_component;

  return (close_button_ && close_button_->GetMirroredBounds().Contains(point))
             ? HTCLOSE
             : HTNOWHERE;
}

void ImeWindowFrameView::GetWindowMask(const gfx::Size& size,
                                       SkPath* window_mask) {
  int width = size.width();
  int height = size.height();

  window_mask->moveTo(0, 0);
  window_mask->lineTo(SkIntToScalar(width), 0);
  window_mask->lineTo(SkIntToScalar(width), SkIntToScalar(height));
  window_mask->lineTo(0, SkIntToScalar(height));

  window_mask->close();
}

void ImeWindowFrameView::ResetWindowControls() {}

void ImeWindowFrameView::UpdateWindowIcon() {
  title_icon_->SchedulePaint();
}

void ImeWindowFrameView::UpdateWindowTitle() {}

void ImeWindowFrameView::SizeConstraintsChanged() {}

gfx::Size ImeWindowFrameView::CalculatePreferredSize() const {
  gfx::Size pref_size =
      ime_window_view_->window()->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref_size.width(), pref_size.height());
  return ime_window_view_->window()
                         ->non_client_view()
                         ->GetWindowBoundsForClientBounds(bounds)
                         .size();
}

gfx::Size ImeWindowFrameView::GetMinimumSize() const {
  return ime_window_view_->GetMinimumSize();
}

gfx::Size ImeWindowFrameView::GetMaximumSize() const {
  return ime_window_view_->GetMaximumSize();
}

void ImeWindowFrameView::OnPaint(gfx::Canvas* canvas) {
  PaintFrameBackground(canvas);
}

bool ImeWindowFrameView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    gfx::Point mouse_location = event.location();
    views::View::ConvertPointToScreen(this, &mouse_location);
    return ime_window_view_->OnTitlebarPointerPressed(
        mouse_location, ImeWindowView::PointerType::MOUSE);
  }
  return false;
}

bool ImeWindowFrameView::OnMouseDragged(const ui::MouseEvent& event) {
  gfx::Point mouse_location = event.location();
  views::View::ConvertPointToScreen(this, &mouse_location);
  return ime_window_view_->OnTitlebarPointerDragged(
      mouse_location, ImeWindowView::PointerType::MOUSE);
}

void ImeWindowFrameView::OnMouseReleased(const ui::MouseEvent& event) {
  ime_window_view_->OnTitlebarPointerReleased(
      ImeWindowView::PointerType::MOUSE);
}

void ImeWindowFrameView::OnMouseCaptureLost() {
  ime_window_view_->OnTitlebarPointerCaptureLost();
}

void ImeWindowFrameView::OnGestureEvent(ui::GestureEvent* event) {
  bool handled = false;
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      gfx::Point loc(event->location());
      views::View::ConvertPointToScreen(this, &loc);
      handled = ime_window_view_->OnTitlebarPointerPressed(
          loc, ImeWindowView::PointerType::TOUCH);
      break;
    }

    case ui::ET_GESTURE_SCROLL_UPDATE: {
      gfx::Point loc(event->location());
      views::View::ConvertPointToScreen(this, &loc);
      handled = ime_window_view_->OnTitlebarPointerDragged(
          loc, ImeWindowView::PointerType::TOUCH);
      break;
    }

    case ui::ET_GESTURE_END:
      ime_window_view_->OnTitlebarPointerReleased(
          ImeWindowView::PointerType::TOUCH);
      handled = true;
      break;

    default:
      break;
  }
  if (handled)
    event->SetHandled();
}

void ImeWindowFrameView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  DCHECK_EQ(close_button_, sender);
  ime_window_view_->OnCloseButtonClicked();
}

void ImeWindowFrameView::PaintFrameBackground(gfx::Canvas* canvas) {
  canvas->DrawColor(kImeBackgroundColor);

  // left border.
  canvas->FillRect(gfx::Rect(0, 0, kImeBorderThickness, height()),
                   kBorderColor);
  // top border.
  canvas->FillRect(gfx::Rect(0, 0, width(), kImeBorderThickness), kBorderColor);
  // right border.
  canvas->FillRect(gfx::Rect(width() - kImeBorderThickness, 0,
                             kImeBorderThickness, height()),
                   kBorderColor);
  // bottom border.
  canvas->FillRect(gfx::Rect(0, height() - kImeBorderThickness, width(),
                             kImeBorderThickness),
                   kBorderColor);
}

}  // namespace ui

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/window_shape.h"

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kPipDisallowReturnToOpenerKey, false)

namespace {

// These constants mirror PictureInPictureBrowserFrameView, which this class
// replaces for the standalone document PiP path.
constexpr int kButtonIconSize = 16;
constexpr int kTopControlsHeight = 34;
constexpr int kResizeBorder = 10;
constexpr int kResizeAreaCornerSize = 16;

// Creates an ImageButton styled for the PiP title bar.
std::unique_ptr<views::ImageButton> CreatePipTitleBarButton(
    const gfx::VectorIcon& icon,
    const std::u16string& tooltip,
    views::Button::PressedCallback callback) {
  auto button = std::make_unique<views::ImageButton>(std::move(callback));
  button->SetImageModel(views::Button::STATE_NORMAL,
                        ui::ImageModel::FromVectorIcon(
                            icon, kColorPipWindowForeground, kButtonIconSize));
  button->SetTooltipText(tooltip);
  button->GetViewAccessibility().SetName(tooltip);
  return button;
}

}  // namespace

DocumentPipFrameView::DocumentPipFrameView(views::Widget* widget) {
  const bool disallow_return_to_opener =
      widget->GetProperty(kPipDisallowReturnToOpenerKey);
  // Create the top bar container.
  AddChildView(views::Builder<views::FlexLayoutView>()
                   .CopyAddressTo(&top_bar_container_view_)
                   .SetOrientation(views::LayoutOrientation::kHorizontal)
                   .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
                   .Build());

  top_bar_container_view_->SetBackground(
      views::CreateSolidBackground(kColorPipWindowTopBarBackground));

  // Create the window title.
  auto title_label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetElideBehavior(gfx::ELIDE_HEAD);
  // Temporary left padding until a location icon view and content setting
  // image views are added to the top bar (which will provide natural spacing).
  title_label->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 8, 0, 0));
  title_label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  title_label->SetBackgroundColor(kColorPipWindowTopBarBackground);
  title_label->SetEnabledColor(kColorPipWindowForeground);
  window_title_ = top_bar_container_view_->AddChildView(std::move(title_label));

  // Create a container for the top right buttons.
  button_container_view_ = top_bar_container_view_->AddChildView(
      std::make_unique<views::FlexLayoutView>());

  // Create the back-to-tab button if allowed.

  if (!disallow_return_to_opener) {
    const auto back_to_tab_cb = [](DocumentPipFrameView* frame_view) {
      frame_view->set_close_reason(
          DocumentPipFrameView::CloseReason::kBackToTabButton);
      if (!PictureInPictureWindowManager::GetInstance()
               ->ExitPictureInPictureViaWindowUi(
                   PictureInPictureWindowManager::UiBehavior::
                       kCloseWindowAndFocusOpener)) {
        frame_view->GetWidget()->Close();
      }
    };
    const auto& back_icon = features::IsRoundedIconsEnabled()
                                ? vector_icons::kBackToTabIcon
                                : vector_icons::kBackToTabChromeRefreshOldIcon;
    back_to_tab_button_ =
        button_container_view_->AddChildView(CreatePipTitleBarButton(
            back_icon,
            l10n_util::GetStringUTF16(
                IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT),
            // Safety: The widget owns the frame view and its child buttons,
            // so the callback cannot outlive the widget.
            base::BindRepeating(back_to_tab_cb, base::Unretained(this))));
  }

  // Create the close button.
  const auto close_cb = [](DocumentPipFrameView* frame_view) {
    frame_view->set_close_reason(
        DocumentPipFrameView::CloseReason::kCloseButton);
    if (!PictureInPictureWindowManager::GetInstance()
             ->ExitPictureInPictureViaWindowUi(
                 PictureInPictureWindowManager::UiBehavior::kCloseWindowOnly)) {
      frame_view->GetWidget()->Close();
    }
  };
  const auto& close_icon = features::IsRoundedIconsEnabled()
                               ? vector_icons::kCloseIcon
                               : vector_icons::kCloseChromeRefreshOldIcon;
  close_image_button_ =
      button_container_view_->AddChildView(CreatePipTitleBarButton(
          close_icon,
          l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_CLOSE_CONTROL_TEXT),
          // Safety: The widget owns the frame view and its child buttons,
          // so the callback cannot outlive the widget.
          base::BindRepeating(close_cb, base::Unretained(this))));

  // TODO(crbug.com/40279642): Don't force dark mode once we support a
  // light mode window.
  widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kDark);
}

DocumentPipFrameView::~DocumentPipFrameView() {
  base::UmaHistogramEnumeration("Media.DocumentPictureInPicture.CloseReason",
                                close_reason_);
}

gfx::Rect DocumentPipFrameView::GetBoundsForClientView() const {
  const auto border_thickness = FrameBorderInsets();
  const int top_height = GetTopAreaHeight();
  return gfx::Rect(border_thickness.left(), top_height,
                   width() - border_thickness.width(),
                   height() - top_height - border_thickness.bottom());
}

gfx::Rect DocumentPipFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  const auto border_thickness = FrameBorderInsets();
  const int top_height = GetTopAreaHeight();
  return gfx::Rect(
      client_bounds.x() - border_thickness.left(),
      client_bounds.y() - top_height,
      client_bounds.width() + border_thickness.width(),
      client_bounds.height() + top_height + border_thickness.bottom());
}

int DocumentPipFrameView::NonClientHitTest(const gfx::Point& point) {
  // Convert control bounds to frame-view coordinates for correct hit testing.
  // Button bounds are in container-local coordinates; convert to frame-view
  // coordinates so NonClientHitTest comparisons are in the same space.
  auto convert_to_frame_coords = [this](views::View* view) -> gfx::Rect {
    if (!view) {
      return gfx::Rect();
    }
    gfx::RectF bounds(view->GetMirroredBounds());
    views::View::ConvertRectToTarget(view->parent(), this, &bounds);
    return gfx::ToEnclosingRect(bounds);
  };

  // Allow interacting with the buttons.
  if (back_to_tab_button_ &&
      convert_to_frame_coords(back_to_tab_button_).Contains(point)) {
    return HTCLIENT;
  }
  if (convert_to_frame_coords(close_image_button_).Contains(point)) {
    return HTCLIENT;
  }

  // Allow dragging and resizing the window.
  int window_component = GetHTComponentForFrame(
      point, ResizeBorderInsets(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      GetWidget()->widget_delegate()->CanResize());
  if (window_component != HTNOWHERE) {
    return window_component;
  }

  // Allow interacting with the web contents.
  if (GetWidget()->client_view()) {
    int frame_component = GetWidget()->client_view()->NonClientHitTest(point);
    if (frame_component != HTNOWHERE) {
      return frame_component;
    }
  }

  return HTCAPTION;
}

void DocumentPipFrameView::GetWindowMask(const gfx::Size& size,
                                         SkPath* window_mask) {
  DCHECK(window_mask);
  views::GetDefaultWindowMask(size, window_mask);
}

gfx::Size DocumentPipFrameView::GetMinimumSize() const {
  return PictureInPictureWindowManager::GetMinimumInnerWindowSize() +
         GetNonClientViewAreaSize();
}

gfx::Size DocumentPipFrameView::GetMaximumSize() const {
  if (!GetWidget() || !GetWidget()->GetNativeWindow()) {
    return GetMinimumSize();
  }

  auto display = display::Screen::Get()->GetDisplayNearestWindow(
      GetWidget()->GetNativeWindow());
  return PictureInPictureWindowManager::GetMaximumWindowSize(display);
}

void DocumentPipFrameView::Layout(PassKey) {
  gfx::Rect content_area = GetLocalBounds();
  content_area.Inset(FrameBorderInsets());
  gfx::Rect top_bar = content_area;
  top_bar.set_height(kTopControlsHeight);
  top_bar_container_view_->SetBoundsRect(top_bar);

  LayoutSuperclass<views::FrameView>(this);
}

int DocumentPipFrameView::GetTopAreaHeight() const {
  return FrameBorderInsets().top() + kTopControlsHeight;
}

gfx::Insets DocumentPipFrameView::FrameBorderInsets() const {
  return gfx::Insets();
}

gfx::Insets DocumentPipFrameView::ResizeBorderInsets() const {
  return gfx::Insets(kResizeBorder);
}

gfx::Size DocumentPipFrameView::GetNonClientViewAreaSize() const {
  const auto border_thickness = FrameBorderInsets();
  const int top_height = GetTopAreaHeight();
  return gfx::Size(border_thickness.width(),
                   top_height + border_thickness.bottom());
}

BEGIN_METADATA(DocumentPipFrameView)
END_METADATA

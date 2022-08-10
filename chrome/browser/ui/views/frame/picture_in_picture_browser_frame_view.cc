// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/window_shape.h"

namespace {

// TODO(https://crbug.com/1346734): Check whether any of the below should be
// based on platform constants instead.

constexpr int kWindowIconImageSize = 14;
constexpr int kBackToTabImageSize = 14;

// The height of the controls bar at the top of the window.
constexpr int kTopControlsHeight = 30;

constexpr int kWindowBorderThickness = 5;
constexpr int kResizeAreaCornerSize = 10;

// The window has a standard Chrome minimum size and does not have a maximum
// size.
constexpr gfx::Size kMinWindowSize(500, 500);

class BackToTabButton : public OverlayWindowImageButton {
 public:
  METADATA_HEADER(BackToTabButton);

  explicit BackToTabButton(PressedCallback callback)
      : OverlayWindowImageButton(std::move(callback)) {
    SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            kBackToTabIcon, kColorPipWindowForeground, kBackToTabImageSize));

    const std::u16string back_to_tab_button_label = l10n_util::GetStringUTF16(
        IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT);
    SetTooltipText(back_to_tab_button_label);
  }
  BackToTabButton(const BackToTabButton&) = delete;
  BackToTabButton& operator=(const BackToTabButton&) = delete;
  ~BackToTabButton() override = default;
};

BEGIN_METADATA(BackToTabButton, OverlayWindowImageButton)
END_METADATA

}  // namespace

PictureInPictureBrowserFrameView::PictureInPictureBrowserFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
  // Creates a window background with solid color.
  // TODO(https://crbug.com/1346734): Need to figure out how to make this
  // background color not overlap pip content. AddChildView() would cause it to
  // overlap while now it never shows.
  window_background_view_ = browser_view->contents_web_view()->AddChildViewAt(
      std::make_unique<views::View>(), 0);
  window_background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  window_background_view_->layer()->SetName("WindowBackgroundView");

  // Creates a view that will hold all the control views.
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .CopyAddressTo(&controls_container_view_)
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .Build());
  controls_container_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  controls_container_view_->layer()->SetName("ControlsContainerView");

  // Creates the window icon.
  controls_container_view_->AddChildView(
      views::Builder<views::ImageView>()
          .CopyAddressTo(&window_icon_)
          .SetImage(ui::ImageModel::FromVectorIcon(
              vector_icons::kHttpsValidIcon, kColorOmniboxSecurityChipSecure,
              kWindowIconImageSize))
          .Build());
  window_icon_->SetPaintToLayer(ui::LAYER_TEXTURED);
  window_icon_->layer()->SetFillsBoundsOpaquely(false);
  window_icon_->layer()->SetName("WindowIcon");

  // Creates the window title.
  controls_container_view_->AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&window_title_)
          .SetText(browser_view->GetWindowTitle())
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .Build());
  controls_container_view_->SetFlexForView(window_title_, 1);
  window_title_->SetPaintToLayer(ui::LAYER_TEXTURED);
  window_title_->layer()->SetName("WindowTitle");

  // Creates the back to tab button.
  back_to_tab_button_ = controls_container_view_->AddChildView(
      std::make_unique<BackToTabButton>(base::BindRepeating(
          [](PictureInPictureBrowserFrameView* frame_view) {
            // TODO(https://crbug.com/1346734): Implement functionality.
          },
          base::Unretained(this))));
  back_to_tab_button_->SetPaintToLayer(ui::LAYER_TEXTURED);
  back_to_tab_button_->layer()->SetFillsBoundsOpaquely(false);
  back_to_tab_button_->layer()->SetName("BackToTabButton");

  // Creates the close button.
  close_image_button_ = controls_container_view_->AddChildView(
      std::make_unique<CloseImageButton>(base::BindRepeating(
          [](PictureInPictureBrowserFrameView* frame_view) {
            frame_view->frame()->CloseWithReason(
                views::Widget::ClosedReason::kCloseButtonClicked);
          },
          base::Unretained(this))));
  close_image_button_->SetPaintToLayer(ui::LAYER_TEXTURED);
  close_image_button_->layer()->SetFillsBoundsOpaquely(false);
  close_image_button_->layer()->SetName("CloseImageButton");
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView implementations:

gfx::Rect PictureInPictureBrowserFrameView::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  return gfx::Rect();
}

int PictureInPictureBrowserFrameView::GetTopInset(bool restored) const {
  return kTopControlsHeight;
}

int PictureInPictureBrowserFrameView::GetThemeBackgroundXInset() const {
  return 0;
}

gfx::Rect PictureInPictureBrowserFrameView::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect PictureInPictureBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return bounds();
}

int PictureInPictureBrowserFrameView::NonClientHitTest(
    const gfx::Point& point) {
  // Do nothing if the click is outside the window.
  if (!bounds().Contains(point))
    return HTNOWHERE;

  // Allow interacting with the buttons.
  if (GetBackToTabControlsBounds().Contains(point) ||
      GetCloseControlsBounds().Contains(point))
    return HTCLIENT;

  // Allow dragging and resizing the window.
  int window_component = GetHTComponentForFrame(
      point, gfx::Insets(kWindowBorderThickness), kResizeAreaCornerSize,
      kResizeAreaCornerSize, GetWidget()->widget_delegate()->CanResize());
  if (window_component != HTNOWHERE)
    return window_component;

  // Allow interacting with the web contents.
  int frame_component = frame()->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  return HTCAPTION;
}

void PictureInPictureBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                                     SkPath* window_mask) {
  DCHECK(window_mask);
  views::GetDefaultWindowMask(size, window_mask);
}

void PictureInPictureBrowserFrameView::UpdateWindowTitle() {
  window_title_->SchedulePaint();
}

gfx::Size PictureInPictureBrowserFrameView::GetMinimumSize() const {
  return kMinWindowSize;
}

void PictureInPictureBrowserFrameView::OnThemeChanged() {
  BrowserNonClientFrameView::OnThemeChanged();

  const auto* color_provider = GetColorProvider();
  window_background_view_->layer()->SetColor(
      color_provider->GetColor(kColorPipWindowBackground));
  controls_container_view_->layer()->SetColor(
      SkColorSetA(color_provider->GetColor(kColorPipWindowControlsBackground),
                  SK_AlphaOPAQUE));

  // Must set an opaque background color for the label before setting opacity.
  window_title_->SetBackgroundColor(
      color_provider->GetColor(kColorPipWindowControlsBackground));
  window_title_->SetEnabledColor(
      color_provider->GetColor(kColorPipWindowForeground));
  window_title_->layer()->SetFillsBoundsOpaquely(false);
}

gfx::Rect PictureInPictureBrowserFrameView::GetBackToTabControlsBounds() const {
  DCHECK(back_to_tab_button_);
  return back_to_tab_button_->GetMirroredBounds();
}

gfx::Rect PictureInPictureBrowserFrameView::GetCloseControlsBounds() const {
  DCHECK(close_image_button_);
  return close_image_button_->GetMirroredBounds();
}

void PictureInPictureBrowserFrameView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  window_background_view_->SetBoundsRect(
      gfx::Rect(gfx::Point(), bounds().size()));
  controls_container_view_->SetBoundsRect(
      gfx::Rect(0, 0, width(), kTopControlsHeight));
}

BEGIN_METADATA(PictureInPictureBrowserFrameView, BrowserNonClientFrameView)
END_METADATA

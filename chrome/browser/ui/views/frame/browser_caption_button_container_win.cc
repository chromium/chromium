// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_caption_button_container_win.h"

#include <memory>

#include "chrome/browser/ui/frame/window_frame_util.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_win.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/windows_caption_button.h"
#include "chrome/browser/win/titlebar_config.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<WindowsCaptionButton> CreateCaptionButton(
    views::Button::PressedCallback callback,
    BrowserFrameViewWin* frame_view,
    ViewID button_type,
    int accessible_name_resource_id) {
  return std::make_unique<WindowsCaptionButton>(
      std::move(callback), frame_view, button_type,
      l10n_util::GetStringUTF16(accessible_name_resource_id));
}

bool HitTestCaptionButton(WindowsCaptionButton* button,
                          const gfx::Point& point) {
  return button && button->GetVisible() && button->bounds().Contains(point);
}

}  // anonymous namespace

BrowserCaptionButtonContainer::BrowserCaptionButtonContainer(
    BrowserFrameViewWin* frame_view)
    : frame_view_(frame_view),
      minimize_button_(AddChildView(CreateCaptionButton(
          base::BindRepeating(&BrowserFrame::Minimize,
                              base::Unretained(frame_view_->frame())),
          frame_view_,
          VIEW_ID_MINIMIZE_BUTTON,
          IDS_APP_ACCNAME_MINIMIZE))),
      maximize_button_(AddChildView(CreateCaptionButton(
          base::BindRepeating(&BrowserFrame::Maximize,
                              base::Unretained(frame_view_->frame())),
          frame_view_,
          VIEW_ID_MAXIMIZE_BUTTON,
          IDS_APP_ACCNAME_MAXIMIZE))),
      restore_button_(AddChildView(CreateCaptionButton(
          base::BindRepeating(&BrowserFrame::Restore,
                              base::Unretained(frame_view_->frame())),
          frame_view_,
          VIEW_ID_RESTORE_BUTTON,
          IDS_APP_ACCNAME_RESTORE))),
      close_button_(AddChildView(CreateCaptionButton(
          base::BindRepeating(&BrowserFrame::CloseWithReason,
                              base::Unretained(frame_view_->frame()),
                              views::Widget::ClosedReason::kCloseButtonClicked),
          frame_view_,
          VIEW_ID_CLOSE_BUTTON,
          IDS_APP_ACCNAME_CLOSE))) {
  // Layout is horizontal, with buttons placed at the trailing end of the view.
  // This allows the container to expand to become a faux titlebar/drag handle.
  auto* const layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /* adjust_width_for_height */ false,
                                   views::MinimumFlexSizeRule::kScaleToZero));

  if (frame_view_->browser_view()->AppUsesWindowControlsOverlay()) {
    UpdateButtonToolTipsForWindowControlsOverlay();
  }
}

BrowserCaptionButtonContainer::~BrowserCaptionButtonContainer() = default;

int BrowserCaptionButtonContainer::NonClientHitTest(
    const gfx::Point& point) const {
  DCHECK(HitTestPoint(point))
      << "should only be called with a point inside this view's bounds";
  // BrowserView covers the frame view when Window Controls Overlay is enabled.
  // The native window that encompasses Web Contents gets the mouse events meant
  // for the caption buttons, so returning HTClient allows these buttons to be
  // highlighted on hover.
  if (frame_view_->browser_view()->IsWindowControlsOverlayEnabled() &&
      (HitTestCaptionButton(minimize_button_, point) ||
       HitTestCaptionButton(maximize_button_, point) ||
       HitTestCaptionButton(restore_button_, point) ||
       HitTestCaptionButton(close_button_, point))) {
    return HTCLIENT;
  }
  if (HitTestCaptionButton(minimize_button_, point)) {
    return HTMINBUTTON;
  }
  if (HitTestCaptionButton(maximize_button_, point)) {
    return HTMAXBUTTON;
  }
  if (HitTestCaptionButton(restore_button_, point)) {
    return HTMAXBUTTON;
  }
  if (HitTestCaptionButton(close_button_, point)) {
    return HTCLOSE;
  }
  return HTCAPTION;
}

void BrowserCaptionButtonContainer::OnWindowControlsOverlayEnabledChanged() {
  if (frame_view_->browser_view()->IsWindowControlsOverlayEnabled()) {
    SetBackground(
        views::CreateSolidBackground(frame_view_->GetTitlebarColor()));

    // BrowserView paints to a layer, so this view must do the same to ensure
    // that it paints on top of the BrowserView.
    SetPaintToLayer();
  } else {
    SetBackground(nullptr);
    DestroyLayer();
  }
  UpdateButtonToolTipsForWindowControlsOverlay();
}

void BrowserCaptionButtonContainer::OnThemeChanged() {
  if (frame_view_->browser_view()->IsWindowControlsOverlayEnabled()) {
    SetBackground(
        views::CreateSolidBackground(frame_view_->GetTitlebarColor()));
  }
  views::View::OnThemeChanged();
}

void BrowserCaptionButtonContainer::ResetWindowControls() {
  minimize_button_->SetState(views::Button::STATE_NORMAL);
  maximize_button_->SetState(views::Button::STATE_NORMAL);
  restore_button_->SetState(views::Button::STATE_NORMAL);
  close_button_->SetState(views::Button::STATE_NORMAL);
  InvalidateLayout();
}

void BrowserCaptionButtonContainer::AddedToWidget() {
  views::Widget* const widget = GetWidget();

  DCHECK(!widget_observation_.IsObserving());
  widget_observation_.Observe(widget);

  UpdateButtons();

  if (frame_view_->browser_view()->IsWindowControlsOverlayEnabled()) {
    SetBackground(
        views::CreateSolidBackground(frame_view_->GetTitlebarColor()));
    // BrowserView paints to a layer, so this must do the same to ensure that it
    // paints on top of the BrowserView.
    SetPaintToLayer();
  }
}

void BrowserCaptionButtonContainer::RemovedFromWidget() {
  DCHECK(widget_observation_.IsObserving());
  widget_observation_.Reset();
}

void BrowserCaptionButtonContainer::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  UpdateButtons();
}

void BrowserCaptionButtonContainer::UpdateButtons() {
  if (!ShouldBrowserCustomDrawTitlebar(frame_view_->browser_view())) {
    minimize_button_->SetVisible(false);
    maximize_button_->SetVisible(false);
    restore_button_->SetVisible(false);
    close_button_->SetVisible(false);
    return;
  }

  minimize_button_->SetVisible(frame_view_->browser_view()->CanMinimize());

  const bool is_maximized = frame_view_->IsMaximized();
  const bool can_maximize = frame_view_->browser_view()->CanMaximize();
  restore_button_->SetVisible(is_maximized && can_maximize);
  maximize_button_->SetVisible(!is_maximized && can_maximize);

  close_button_->SetVisible(true);

  // In touch mode, windows cannot be taken out of fullscreen or tiled mode, so
  // the maximize/restore button should be disabled, unless the window is not
  // maximized. TODO(crbug.com/40849150): Also check if the window is tiled.
  const bool is_touch = ui::TouchUiController::Get()->touch_ui();
  restore_button_->SetEnabled(!is_touch);
  maximize_button_->SetEnabled(!is_touch || !is_maximized);
}

void BrowserCaptionButtonContainer::
    UpdateButtonToolTipsForWindowControlsOverlay() {
  if (frame_view_->browser_view()->IsWindowControlsOverlayEnabled()) {
    minimize_button_->SetTooltipText(
        minimize_button_->GetViewAccessibility().GetCachedName());
    maximize_button_->SetTooltipText(
        maximize_button_->GetViewAccessibility().GetCachedName());
    restore_button_->SetTooltipText(
        restore_button_->GetViewAccessibility().GetCachedName());
    close_button_->SetTooltipText(
        close_button_->GetViewAccessibility().GetCachedName());
  } else {
    minimize_button_->SetTooltipText(u"");
    maximize_button_->SetTooltipText(u"");
    restore_button_->SetTooltipText(u"");
    close_button_->SetTooltipText(u"");
  }
}

BEGIN_METADATA(BrowserCaptionButtonContainer)
END_METADATA

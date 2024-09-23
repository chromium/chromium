// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/non_client_frame_view_base.h"

#include <memory>

#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/frame/header_view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

NonClientFrameViewBase::OverlayView::OverlayView(HeaderView* header_view)
    : header_view_(header_view) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

NonClientFrameViewBase::OverlayView::~OverlayView() = default;

void NonClientFrameViewBase::OverlayView::Layout(PassKey) {
  // Layout |header_view_| because layout affects the result of
  // GetPreferredOnScreenHeight().
  header_view_->DeprecatedLayoutImmediately();

  int onscreen_height = header_view_->GetPreferredOnScreenHeight();
  int height = header_view_->GetPreferredHeight();
  if (onscreen_height == 0 || !GetVisible()) {
    header_view_->SetVisible(false);
    // Make sure the correct width is set even when immersive is enabled, but
    // never revealed yet.
    header_view_->SetBounds(0, 0, width(), height);
  } else {
    header_view_->SetBounds(0, onscreen_height - height, width(), height);
    header_view_->SetVisible(true);
  }
}

bool NonClientFrameViewBase::OverlayView::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  // Grab events in the header view. Return false for other events so that they
  // can be handled by the client view.
  return header_view_->HitTestRect(rect);
}

BEGIN_METADATA(NonClientFrameViewBase, OverlayView)
END_METADATA

NonClientFrameViewBase::NonClientFrameViewBase(views::Widget* frame)
    : frame_(frame) {
  DCHECK(frame_);

  auto header_view = std::make_unique<HeaderView>(frame_, this);
  header_view->Init();

  auto overlay_view = std::make_unique<OverlayView>(header_view.get());
  header_view_ = overlay_view->AddChildView(std::move(header_view));
  overlay_view_ = overlay_view.get();

  // |header_view_| is set as the non client view's overlay view so that it can
  // overlay the web contents in immersive fullscreen.
  // TODO(pkasting): Consider having something like NonClientViewAsh, which
  // would avoid the need to expose an "overlay view" concept on the
  // cross-platform class, and might allow for simpler creation/ownership/
  // plumbing.
  frame->non_client_view()->SetOverlayView(overlay_view.release());

  UpdateDefaultFrameColors();
}

NonClientFrameViewBase::~NonClientFrameViewBase() = default;

HeaderView* NonClientFrameViewBase::GetHeaderView() {
  return header_view_;
}

int NonClientFrameViewBase::NonClientTopBorderHeight() const {
  const aura::Window* frame_window = frame_->GetNativeWindow();
  const WindowStateType window_state_type =
      frame_window->GetProperty(kWindowStateTypeKey);
  const bool is_in_tablet_mode = display::Screen::GetScreen()->InTabletMode();
  const bool should_have_frame_in_tablet =
      window_state_type == chromeos::WindowStateType::kFloated ||
      window_state_type == chromeos::WindowStateType::kNormal;
  // The frame should not occupy the window area when it's in fullscreen,
  // not visible, disabled, in immersive mode or in tablet mode.
  // TODO(crbug.com/40879470): Support NonClientFrameViewAshImmersiveHelper on
  // Lacros so that we can remove InTabletMode() && IsMaximized() condition.
  if (frame_->IsFullscreen() || !GetFrameEnabled() ||
      header_view_->in_immersive_mode() ||
      (is_in_tablet_mode && !should_have_frame_in_tablet)) {
    return 0;
  }
  return header_view_->GetPreferredHeight();
}

gfx::Rect NonClientFrameViewBase::GetBoundsForClientView() const {
  gfx::Rect client_bounds = bounds();
  client_bounds.Inset(gfx::Insets::TLBR(NonClientTopBorderHeight(), 0, 0, 0));
  return client_bounds;
}

gfx::Rect NonClientFrameViewBase::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(gfx::Insets::TLBR(-NonClientTopBorderHeight(), 0, 0, 0));
  return window_bounds;
}

int NonClientFrameViewBase::NonClientHitTest(const gfx::Point& point) {
  return FrameBorderNonClientHitTest(this, point);
}

void NonClientFrameViewBase::GetWindowMask(const gfx::Size& size,
                                           SkPath* window_mask) {
  // No window masks in Aura.
}

void NonClientFrameViewBase::ResetWindowControls() {
  header_view_->ResetWindowControls();
}

void NonClientFrameViewBase::UpdateWindowIcon() {}

void NonClientFrameViewBase::UpdateWindowTitle() {
  header_view_->SchedulePaintForTitle();
}

void NonClientFrameViewBase::SizeConstraintsChanged() {
  header_view_->UpdateCaptionButtons();
}

views::View::Views NonClientFrameViewBase::GetChildrenInZOrder() {
  return header_view_->GetFrameHeader()->GetAdjustedChildrenInZOrder(this);
}

gfx::Size NonClientFrameViewBase::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size pref = frame_->client_view()->GetPreferredSize(available_size);
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return frame_->non_client_view()
      ->GetWindowBoundsForClientBounds(bounds)
      .size();
}

void NonClientFrameViewBase::Layout(PassKey) {
  LayoutSuperclass<views::NonClientFrameView>(this);
  if (!GetFrameEnabled())
    return;
  aura::Window* frame_window = frame_->GetNativeWindow();
  frame_window->SetProperty(aura::client::kTopViewInset,
                            NonClientTopBorderHeight());
}

gfx::Size NonClientFrameViewBase::GetMinimumSize() const {
  if (!GetFrameEnabled())
    return gfx::Size();

  gfx::Size min_client_view_size(frame_->client_view()->GetMinimumSize());
  return gfx::Size(
      std::max(header_view_->GetMinimumWidth(), min_client_view_size.width()),
      NonClientTopBorderHeight() + min_client_view_size.height());
}

gfx::Size NonClientFrameViewBase::GetMaximumSize() const {
  gfx::Size max_client_size(frame_->client_view()->GetMaximumSize());
  int width = 0;
  int height = 0;

  if (max_client_size.width() > 0)
    width = std::max(header_view_->GetMinimumWidth(), max_client_size.width());
  if (max_client_size.height() > 0)
    height = NonClientTopBorderHeight() + max_client_size.height();

  return gfx::Size(width, height);
}

void NonClientFrameViewBase::OnThemeChanged() {
  NonClientFrameView::OnThemeChanged();
  UpdateDefaultFrameColors();
}

void NonClientFrameViewBase::UpdateDefaultFrameColors() {
  aura::Window* frame_window = frame_->GetNativeWindow();
  if (!frame_window->GetProperty(kTrackDefaultFrameColors))
    return;

  auto* color_provider = frame_->GetColorProvider();

  frame_window->SetProperty(kFrameActiveColorKey,
                            color_provider->GetColor(ui::kColorFrameActive));
  frame_window->SetProperty(kFrameInactiveColorKey,
                            color_provider->GetColor(ui::kColorFrameInactive));
}

bool NonClientFrameViewBase::DoesIntersectRect(const views::View* target,
                                               const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);

  // Give the OverlayView the first chance to handle events.
  if (frame_enabled_ && overlay_view_->HitTestRect(rect))
    return false;

  // Handle the event if it's within the bounds of the ClientView.
  gfx::RectF rect_in_client_view_coords_f(rect);
  View::ConvertRectToTarget(this, frame_->client_view(),
                            &rect_in_client_view_coords_f);
  gfx::Rect rect_in_client_view_coords =
      gfx::ToEnclosingRect(rect_in_client_view_coords_f);
  return frame_->client_view()->HitTestRect(rect_in_client_view_coords);
}

void NonClientFrameViewBase::PaintAsActiveChanged() {
  header_view_->GetFrameHeader()->SetPaintAsActive(ShouldPaintAsActive());
  frame_->non_client_view()->DeprecatedLayoutImmediately();
}

void NonClientFrameViewBase::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
    case display::TabletState::kInTabletMode:
    case display::TabletState::kInClamshellMode:
      // Without this, Layout is not guaranteed to be called when the tablet
      // state changes. Layout must be called so that the header view can hide
      // or unhide as appropriate.
      InvalidateLayout();
      break;
  }
}

BEGIN_METADATA(NonClientFrameViewBase)
END_METADATA

}  // namespace chromeos

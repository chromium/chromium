// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_rewrite_view.h"

#include "ash/constants/ash_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/input/native_web_keyboard_event.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

constexpr int kMakoAnchorVerticalPadding = 16;
constexpr int kMakoScreenEdgePadding = 16;
constexpr int kMakoRewriteCornerRadius = 20;

constexpr int kMakoInitialWidth = 440;
constexpr int kMakoInitialHeight = 343;

// Height threshold of the mako rewrite UI which determines its screen position.
// Tall UI is centered on the display screen containing the caret, while short
// UI is anchored at the caret.
constexpr int kMakoRewriteHeightThreshold = 400;

// Compute initial widget bounds.
gfx::Rect ComputeInitialWidgetBounds(gfx::Rect caret_bounds,
                                     gfx::Insets inset,
                                     bool can_fallback_to_center_position) {
  gfx::Rect screen_work_area = display::Screen::GetScreen()
                                   ->GetDisplayMatching(caret_bounds)
                                   .work_area();
  screen_work_area.Inset(kMakoScreenEdgePadding);

  gfx::Size initial_size = gfx::Size(kMakoInitialWidth, kMakoInitialHeight);

  // Otherwise, try to place it under at the bottom left of the selection.
  gfx::Rect anchor = caret_bounds;
  anchor.Outset(gfx::Outsets::VH(kMakoAnchorVerticalPadding, 0));

  gfx::Rect mako_contents_bounds =
      can_fallback_to_center_position &&
              (caret_bounds == gfx::Rect() ||
               !screen_work_area.Contains(caret_bounds))
          ? gfx::Rect(screen_work_area.x() + screen_work_area.width() / 2 -
                          initial_size.width() / 2,
                      screen_work_area.y() + screen_work_area.height() / 2 -
                          initial_size.height() / 2,
                      initial_size.width(), initial_size.height())
          : gfx::Rect(anchor.bottom_left(), initial_size);

  // If horizontally offscreen, just move it to the right edge of the screen.
  if (mako_contents_bounds.right() > screen_work_area.right()) {
    mako_contents_bounds.set_x(screen_work_area.right() - initial_size.width());
  }

  // If vertically offscreen, try above the selection.
  if (mako_contents_bounds.bottom() > screen_work_area.bottom()) {
    mako_contents_bounds.set_y(anchor.y() - initial_size.height());
  }

  // If still vertically offscreen, just move it to the bottom of the screen.
  if (mako_contents_bounds.y() < screen_work_area.y()) {
    mako_contents_bounds.set_y(screen_work_area.bottom() -
                               initial_size.height());
  }

  // Compute widget bounds, which includes the border and shadow around the main
  // contents. Then, adjust again to ensure the whole widget is onscreen.
  gfx::Rect widget_bounds(mako_contents_bounds);
  widget_bounds.Inset(inset);
  widget_bounds.AdjustToFit(screen_work_area);

  return widget_bounds;
}

}  // namespace

MakoRewriteView::MakoRewriteView(WebUIContentsWrapper* contents_wrapper,
                                 const gfx::Rect& caret_bounds,
                                 bool can_fallback_to_center_position)
    : WebUIBubbleDialogView(nullptr,
                            contents_wrapper->GetWeakPtr(),
                            std::nullopt,
                            views::BubbleBorder::TOP_RIGHT,
                            false),
      caret_bounds_(caret_bounds),
      can_fallback_to_center_position_(can_fallback_to_center_position) {
  set_has_parent(false);
  set_corner_radius(kMakoRewriteCornerRadius);
  // Disable the default offscreen adjustment so that we can customise it.
  set_adjust_if_offscreen(false);
  event_handler_ = std::make_unique<MakoBubbleEventHandler>(this);
  dragging_initialized_ = false;
  resizing_initialized_ = false;
}

MakoRewriteView::~MakoRewriteView() = default;

// TODO(b/321585827): Remove MakoRewriteView::ResizeDueToAutoResize() once
// resizing support is lanched.
void MakoRewriteView::ResizeDueToAutoResize(content::WebContents* source,
                                            const gfx::Size& new_size) {
  // When resizing support is enabled, we should only correct widget bounds
  // during ShowUI().
  if (base::FeatureList::IsEnabled(ash::features::kOrcaResizingSupport)) {
    return;
  }

  WebUIBubbleDialogView::ResizeDueToAutoResize(source, new_size);
  gfx::Rect screen_work_area = display::Screen::GetScreen()
                                   ->GetDisplayMatching(caret_bounds_)
                                   .work_area();
  screen_work_area.Inset(kMakoScreenEdgePadding);

  // If the contents is very tall, just place it at the center of the screen.
  if (new_size.height() > kMakoRewriteHeightThreshold) {
    SetArrowWithoutResizing(views::BubbleBorder::FLOAT);
    SetAnchorRect(screen_work_area);
    return;
  }

  // Otherwise, try to place it under at the bottom left of the selection.
  gfx::Rect anchor = caret_bounds_;
  anchor.Outset(gfx::Outsets::VH(kMakoAnchorVerticalPadding, 0));

  gfx::Rect mako_contents_bounds =
      can_fallback_to_center_position_ &&
              (caret_bounds_ == gfx::Rect() ||
               !screen_work_area.Contains(caret_bounds_))
          ? gfx::Rect(screen_work_area.x() + screen_work_area.width() / 2 -
                          new_size.width() / 2,
                      screen_work_area.y() + screen_work_area.height() / 2 -
                          new_size.height() / 2,
                      new_size.width(), new_size.height())
          : gfx::Rect(anchor.bottom_left(), new_size);

  // If horizontally offscreen, just move it to the right edge of the screen.
  if (mako_contents_bounds.right() > screen_work_area.right()) {
    mako_contents_bounds.set_x(screen_work_area.right() - new_size.width());
  }

  // If vertically offscreen, try above the selection.
  if (mako_contents_bounds.bottom() > screen_work_area.bottom()) {
    mako_contents_bounds.set_y(anchor.y() - new_size.height());
  }
  // If still vertically offscreen, just move it to the bottom of the screen.
  if (mako_contents_bounds.y() < screen_work_area.y()) {
    mako_contents_bounds.set_y(screen_work_area.bottom() - new_size.height());
  }

  // Compute widget bounds, which includes the border and shadow around the main
  // contents. Then, adjust again to ensure the whole widget is onscreen.
  gfx::Rect widget_bounds(mako_contents_bounds);
  widget_bounds.Inset(-GetBubbleFrameView()->bubble_border()->GetInsets());
  widget_bounds.AdjustToFit(screen_work_area);

  GetWidget()->SetBounds(widget_bounds);
}

void MakoRewriteView::SetContentsBounds(content::WebContents* source,
                                        const gfx::Rect& bounds) {
  if (views::WebView* web_view_ptr = web_view()) {
    web_view_ptr->SetPreferredSize(bounds.size());
  }
  GetWidget()->SetBounds(bounds);
  content_bounds_updated_by_webui_ = true;
}

void MakoRewriteView::ShowUI() {
  WebUIBubbleDialogView::ShowUI();
  // TODO(b/321585877): Remove feature flag for dragging support.
  if (base::FeatureList::IsEnabled(ash::features::kOrcaDraggingSupport)) {
    SetupDraggingSupport();
  }
  if (base::FeatureList::IsEnabled(ash::features::kOrcaResizingSupport)) {
    SetupResizingSupport();
  }
  content_bounds_updated_by_webui_ = false;
}

void MakoRewriteView::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  SkRegion sk_region;
  for (const blink::mojom::DraggableRegionPtr& region : regions) {
    sk_region.op(
        SkIRect::MakeLTRB(region->bounds.x(), region->bounds.y(),
                          region->bounds.x() + region->bounds.width(),
                          region->bounds.y() + region->bounds.height()),
        region->draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }
  draggable_region_ = sk_region;
}

const std::optional<SkRegion> MakoRewriteView::GetDraggableRegion() {
  return draggable_region_;
}

const gfx::Rect MakoRewriteView::GetWidgetBoundsInScreen() {
  views::Widget* widget = GetWidget();
  if (!widget) {
    return gfx::Rect();
  }
  return widget->GetWindowBoundsInScreen();
}

void MakoRewriteView::SetWidgetBoundsConstrained(const gfx::Rect bounds) {
  if (views::WebView* web_view_ptr = web_view()) {
    web_view_ptr->SetPreferredSize(bounds.size());
  }
  if (views::Widget* widget = GetWidget()) {
    widget->SetBoundsConstrained(bounds);
  }
}

void MakoRewriteView::SetCursor(const ui::Cursor& cursor) {
  if (views::Widget* widget = GetWidget()) {
    widget->SetCursor(cursor);
  }
}

bool MakoRewriteView::IsDraggingEnabled() {
  // Once the content bounds is updated by Web UI, we should stop resizing
  // support. This is currently used by Feedback UI.
  if (content_bounds_updated_by_webui_) {
    return false;
  }
  return base::FeatureList::IsEnabled(ash::features::kOrcaDraggingSupport);
}

bool MakoRewriteView::IsResizingEnabled() {
  // Once the content bounds is updated by Web UI, we should stop resizing
  // support. This is currently used by Feedback UI.
  if (content_bounds_updated_by_webui_) {
    return false;
  }
  return base::FeatureList::IsEnabled(ash::features::kOrcaResizingSupport);
}

bool MakoRewriteView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
      event.dom_key == ui::DomKey::ESCAPE) {
    return true;
  }

  return WebUIBubbleDialogView::HandleKeyboardEvent(source, event);
}

void MakoRewriteView::SetupDraggingSupport() {
  // This avoids binding event handler twice.
  if (dragging_initialized_) {
    return;
  }
  dragging_initialized_ = true;
  views::WebView* web_view_ptr = web_view();
  if (!web_view_ptr) {
    return;
  }
  content::WebContents* web_contents = web_view_ptr->GetWebContents();
  if (!web_contents) {
    return;
  }

  // Tell Blink that we support draggable region.
  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  if (rfh) {
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> client;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->SetSupportsDraggableRegions(true);
  }

  // Bind event handlers for dragging support.
  GetWidget()->GetNativeView()->AddPreTargetHandler(event_handler_.get());
}

void MakoRewriteView::SetupResizingSupport() {
  // This avoids setting initial bounds twice.
  if (resizing_initialized_) {
    return;
  }
  // When orca resizing support is enabled, there will be no
  // `ResizeDueToAutoResize` callback and thus we need to setup widget bounds
  // here as part of ShowUI.
  GetWidget()->SetBounds(ComputeInitialWidgetBounds(
      caret_bounds_, -GetBubbleFrameView()->bubble_border()->GetInsets(),
      can_fallback_to_center_position_));
  resizing_initialized_ = true;
}

BEGIN_METADATA(MakoRewriteView)
END_METADATA

}  // namespace ash

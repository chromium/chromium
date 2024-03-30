// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compose/compose_dialog_view.h"

#include <vector>
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "components/compose/core/browser/config.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"

namespace {

gfx::Rect FallbackAtBottomShiftUpToPermanentPosition(
    gfx::Rect best_location,
    const gfx::Rect& screen_work_area,
    const gfx::Size& widget_size) {
  // Make sure all sizes of the view will fit in the work area
  best_location.AdjustToFit(screen_work_area);

  // Now make best_location reflect the actual size of the widget.
  best_location.set_size(widget_size);

  return best_location;
}

gfx::Rect FallbackAtBottomShiftMinimumForVisibility(gfx::Rect best_location) {
  // Just use the initial location anchored at bottom left directly.
  return best_location;
}

gfx::Rect FallbackCenterOnFormFieldStrategy(const gfx::Rect& screen_work_are,
                                            const gfx::Size& widget_size,
                                            const gfx::Rect& anchor_rect) {
  gfx::Rect widget_rect;
  widget_rect.set_size(widget_size);
  gfx::Vector2d translation = anchor_rect.CenterPoint().OffsetFromOrigin() -
                              widget_rect.CenterPoint().OffsetFromOrigin();

  widget_rect.Offset(translation);
  return widget_rect;
}

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kComposeWebviewElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ComposeDialogView, kComposeDialogId);

// static
gfx::Rect ComposeDialogView::CalculateBubbleBounds(
    gfx::Rect screen_work_area,
    gfx::Size widget_size,
    gfx::Rect anchor_bounds,
    std::optional<gfx::Rect> parent_bounds) {
  // Widget can't be smaller than a minimum size.
  widget_size.SetToMax({kComposeMaxDialogWidthPx, kComposeMinDialogHeightPx});

  // We don't want to render anything within `padding` pixels of the edge of the
  // screen work area.
  screen_work_area.Inset(kComposeDialogWorkAreaPadding);

  // If the param to stay in the window bounds is true, and the window is large
  // enough, use that as the work area instead.
  if (compose::GetComposeConfig().stay_in_window_bounds &&
      parent_bounds.has_value()) {
    if ((widget_size.width() <= parent_bounds->width() &&
         widget_size.height() <= parent_bounds->height())) {
      screen_work_area = parent_bounds.value();
    }
  }

  // We don't want to render anything within `padding` pixels of the edge of the
  // anchor rect.  But we will if we have to (due to AdjustToFit below).
  anchor_bounds.Outset(kComposeDialogAnchorPadding);

  // Available space measures the distance from each side of the padded work
  // area to the edge of the padded anchor (plus padding).
  gfx::Insets available_space = screen_work_area.InsetsFrom(anchor_bounds);

  // Ideally we render at the bottom left of the anchor. If the dialog would be
  // offscreen, we reposition it.
  gfx::Rect best_location(
      anchor_bounds.bottom_left(),
      gfx::Size(kComposeMaxDialogWidthPx, kComposeMaxDialogHeightPx));

  bool space_below = available_space.bottom() >= kComposeMaxDialogHeightPx;
  bool space_above = available_space.top() >= kComposeMaxDialogHeightPx;

  // the position of the dialog is determined by finding a suitable location for
  // a maximum-sized ComposeDialogView, so that we know that the dialog doesn't
  // have to switch sides as it resizes. If the dialog is smaller than max, we
  // need to apply an alignment within that larger rectangle.

  if (!space_below) {
    // Not enough room in the preferred location. Try above the anchor.
    if (space_above) {
      // If it is laid out above the anchor rect then it
      // will be bottom-aligned.
      best_location.set_y(anchor_bounds.y() - widget_size.height());
    } else {
      // If not enough space above or below, try one of the following backup
      // strategies:

      switch (compose::GetComposeConfig().positioning_strategy) {
        case compose::DialogFallbackPositioningStrategy::kCenterOnAnchorRect:
          best_location = FallbackCenterOnFormFieldStrategy(
              screen_work_area, widget_size, anchor_bounds);
          break;
        case compose::DialogFallbackPositioningStrategy::kShiftUpUntilOnscreen:
          best_location =
              FallbackAtBottomShiftMinimumForVisibility(best_location);
          break;
        case compose::DialogFallbackPositioningStrategy::
            kShiftUpUntilMaxSizeIsOnscreen:
        default:
          best_location = FallbackAtBottomShiftUpToPermanentPosition(
              best_location, screen_work_area, widget_size);
          break;
      }
    }
  }

  // Always use the size that the WebUI wants to be in the end.
  best_location.set_size(widget_size);

  // Always remain completely on screen within the provided insets.
  best_location.AdjustToFit(screen_work_area);

  return best_location;
}

ComposeDialogView::~ComposeDialogView() = default;

ComposeDialogView::ComposeDialogView(
    View* anchor_view,
    std::unique_ptr<WebUIContentsWrapperT<ComposeUntrustedUI>> bubble_wrapper,
    const gfx::Rect& anchor_bounds,
    views::BubbleBorder::Arrow anchor_position)
    : WebUIBubbleDialogView(anchor_view,
                            bubble_wrapper->GetWeakPtr(),
                            anchor_bounds,
                            anchor_position),
      anchor_bounds_(anchor_bounds),
      bubble_wrapper_(std::move(bubble_wrapper)) {
  SetProperty(views::kElementIdentifierKey, kComposeDialogId);
  web_view()->SetProperty(views::kElementIdentifierKey,
                          kComposeWebviewElementId);
}

void ComposeDialogView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  WebUIBubbleDialogView::OnBeforeBubbleWidgetInit(params, widget);
#if BUILDFLAG(IS_LINUX)
  // In linux, windows may be clipped to their anchors' bounds,
  // resulting in visual errors, unless they use accelerated rendering. See
  // crbug.com/1445770 for details.
  params->use_accelerated_widget_override = true;
#endif
}

gfx::Rect ComposeDialogView::GetBubbleBounds() {
  gfx::Size widget_size = BubbleDialogDelegateView::GetBubbleBounds().size();

  std::optional<gfx::Rect> parent_bounds;
  if (GetWidget()->parent()) {
    parent_bounds = GetWidget()->parent()->GetWindowBoundsInScreen();
  }

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(
          GetAnchorView()->GetWidget()->GetNativeView());
  gfx::Rect screen_work_area = display.work_area();

  return CalculateBubbleBounds(screen_work_area, widget_size, anchor_bounds_,
                               parent_bounds);
}

bool ComposeDialogView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate = ContextMenuDelegate::FromWebContents(
      content::WebContents::FromRenderFrameHost(&render_frame_host));
  DCHECK(menu_delegate);

  std::unique_ptr<RenderViewContextMenuBase> menu =
      menu_delegate->BuildMenu(render_frame_host, params);
  // Remove everything that is not copy, paste, or cut or spellcheck
  // suggestions.
  std::vector<int> command_ids;
  for (size_t index = 0; index < menu->menu_model().GetItemCount(); index++) {
    int command_id = menu->menu_model().GetCommandIdAt(index);
    if ((command_id < IDC_CONTENT_CONTEXT_COPY ||
         command_id > IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE) &&
        (command_id < IDC_SPELLCHECK_SUGGESTION_0 ||
         command_id > IDC_SPELLCHECK_SUGGESTION_LAST) &&
        command_id != IDC_CONTENT_CONTEXT_INSPECTELEMENT && command_id > 0) {
      command_ids.push_back(command_id);
    }
  }

  for (size_t index = 0; index < command_ids.size(); index++) {
    menu->RemoveMenuItem(command_ids[index]);
  }
  menu->RemoveAdjacentSeparators();

  // There's no method to remove the final separator if there is one, so we have
  // to hack around it.
  menu->RemoveSeparatorBeforeMenuItem(IDC_CONTENT_CONTEXT_INSPECTELEMENT);
  menu->RemoveMenuItem(IDC_CONTENT_CONTEXT_INSPECTELEMENT);

  // Only show the menu if there are items in it.
  if (menu->menu_model().GetItemCount() > 0) {
    menu_delegate->ShowMenu(std::move(menu));
  }
  return true;
}

base::WeakPtr<ComposeDialogView> ComposeDialogView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(ComposeDialogView)
END_METADATA

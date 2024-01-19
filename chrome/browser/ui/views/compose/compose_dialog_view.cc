// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compose/compose_dialog_view.h"

#include <vector>
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kComposeDialogWorkAreaPadding = 16;
constexpr int kComposeDialogAnchorPadding = 0;

constexpr int kComposeMaxDialogHeightPx = 366;
constexpr int kComposeMaxDialogWidthPx = 448;

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kComposeWebviewElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ComposeDialogView, kComposeDialogId);

ComposeDialogView::~ComposeDialogView() = default;

ComposeDialogView::ComposeDialogView(
    View* anchor_view,
    std::unique_ptr<BubbleContentsWrapperT<ComposeUI>> bubble_wrapper,
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
  params->requires_accelerated_widget = true;
#endif
}

gfx::Rect ComposeDialogView::GetBubbleBounds() {
  const gfx::Size widget_size =
      BubbleDialogDelegateView::GetBubbleBounds().size();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(
          GetAnchorView()->GetWidget()->GetNativeView());
  gfx::Rect screen_work_area = display.work_area();

  // We don't want to render anything within `padding` pixels of the edge of the
  // screen work area.
  screen_work_area.Inset(kComposeDialogWorkAreaPadding);
  gfx::Rect anchor = anchor_bounds_;

  // We don't want to render anything within `padding` pixels of the edge of the
  // anchor rect.  But we will if we have to (due to AdjustToFit below).
  anchor.Outset(kComposeDialogAnchorPadding);

  // Available space measures the distance from each side of the padded work
  // area to the edge of the padded anchor (plus padding).
  gfx::Insets available_space = screen_work_area.InsetsFrom(anchor);

  // Ideally we render at the bottom left of the anchor.  If the dialog would be
  // offscreen, we reposition it.
  gfx::Rect best_location(anchor.bottom_left(), widget_size);
  if (available_space.bottom() < kComposeMaxDialogHeightPx) {
    // Not enough space in preferred location. Try other locations
    if (available_space.top() >= kComposeMaxDialogHeightPx) {
      best_location.set_y(anchor.y() - best_location.height());
    } else if (available_space.right() > kComposeMaxDialogWidthPx) {
      best_location.set_origin(anchor.top_right());
    } else if (available_space.left() > kComposeMaxDialogWidthPx) {
      best_location.set_x(anchor.x() - best_location.width());
      best_location.set_y(anchor.y());
    } else {
      // fallback, nowhere works cleanly. Try to place it near the top right.
      best_location.set_origin(anchor.top_right());
    }
  }

  best_location.AdjustToFit(screen_work_area);
  return best_location;
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

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_REWRITE_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_REWRITE_VIEW_H_

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/ash/mako/mako_bubble_event_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace ash {

// A view to contain the Mako rewrite UI.
class MakoRewriteView : public WebUIBubbleDialogView,
                        public MakoBubbleEventHandler::Delegate {
  METADATA_HEADER(MakoRewriteView, WebUIBubbleDialogView)

 public:
  MakoRewriteView(WebUIContentsWrapper* contents_wrapper,
                  const gfx::Rect& caret_bounds,
                  bool can_fallback_to_center_position);
  MakoRewriteView(const MakoRewriteView&) = delete;
  MakoRewriteView& operator=(const MakoRewriteView&) = delete;
  ~MakoRewriteView() override;

  // WebUIBubbleDialogView:
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;

  // WebUIContentsWrapper::Host:
  void ShowUI() override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& new_size) override;

  // MakoBubbleEventHandler::Delegate
  const std::optional<SkRegion> GetDraggableRegion() override;
  const gfx::Rect GetWidgetBoundsInScreen() override;
  void SetWidgetBoundsConstrained(const gfx::Rect bounds) override;
  void SetCursor(const ui::Cursor& cursor) override;
  bool IsDraggingEnabled() override;
  bool IsResizingEnabled() override;

  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

 private:
  gfx::Rect caret_bounds_;
  std::optional<SkRegion> draggable_region_ = std::nullopt;
  std::unique_ptr<MakoBubbleEventHandler> event_handler_;
  bool dragging_initialized_;
  bool resizing_initialized_;
  bool content_bounds_updated_by_webui_;
  bool can_fallback_to_center_position_;

  void SetupDraggingSupport();
  void SetupResizingSupport();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_REWRITE_VIEW_H_

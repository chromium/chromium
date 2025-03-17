// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_REWRITE_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_REWRITE_VIEW_H_

#include "chrome/browser/ui/webui/ash/mako/draggable_bubble_dialog_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace ash {

// A view to contain the Mako rewrite UI.
class MakoRewriteView : public DraggableBubbleDialogView {
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
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& new_size) override;

  // MakoBubbleEventHandler::Delegate
  bool IsDraggingEnabled() override;
  bool IsResizingEnabled() override;

  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

 private:
  gfx::Rect caret_bounds_;
  bool resizing_initialized_;
  bool content_bounds_updated_by_webui_;
  bool can_fallback_to_center_position_;

  void SetupResizingSupport();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_REWRITE_VIEW_H_

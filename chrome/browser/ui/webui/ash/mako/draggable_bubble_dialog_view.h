// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_DRAGGABLE_BUBBLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_DRAGGABLE_BUBBLE_DIALOG_VIEW_H_

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/ash/mako/mako_bubble_event_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace ash {

// A bubble dialog view with dragging support.
class DraggableBubbleDialogView : public WebUIBubbleDialogView,
                                  public MakoBubbleEventHandler::Delegate {
  METADATA_HEADER(DraggableBubbleDialogView, WebUIBubbleDialogView)

 public:
  explicit DraggableBubbleDialogView(WebUIContentsWrapper* contents_wrapper);

  DraggableBubbleDialogView(const DraggableBubbleDialogView&) = delete;
  DraggableBubbleDialogView& operator=(const DraggableBubbleDialogView&) =
      delete;
  ~DraggableBubbleDialogView() override;

  // WebUIContentsWrapper::Host:
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;

  // MakoBubbleEventHandler::Delegate
  const std::optional<SkRegion> GetDraggableRegion() override;
  const gfx::Rect GetWidgetBoundsInScreen() override;
  void SetWidgetBoundsConstrained(const gfx::Rect bounds) override;
  void SetCursor(const ui::Cursor& cursor) override;

 protected:
  void SetupDraggingSupport();

 private:
  std::optional<SkRegion> draggable_region_ = std::nullopt;
  std::unique_ptr<MakoBubbleEventHandler> event_handler_;
  bool dragging_initialized_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_DRAGGABLE_BUBBLE_DIALOG_VIEW_H_

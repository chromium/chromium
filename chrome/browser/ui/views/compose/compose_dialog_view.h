// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_VIEW_H_

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/compose/compose_untrusted_ui.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

DECLARE_ELEMENT_IDENTIFIER_VALUE(kComposeWebviewElementId);

// A view for the contents area of the Compose dialog.
class ComposeDialogView : public WebUIBubbleDialogView {
  METADATA_HEADER(ComposeDialogView, WebUIBubbleDialogView)
 public:
  static constexpr int kComposeDialogWorkAreaPadding = 16;
  static constexpr int kComposeDialogAnchorPadding = 0;

  static constexpr int kComposeMaxDialogHeightPx = 366;
  static constexpr int kComposeMinDialogHeightPx = 215;
  static constexpr int kComposeMaxDialogWidthPx = 448;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kComposeDialogId);

  static gfx::Rect CalculateBubbleBounds(
      gfx::Rect screen_work_area,
      gfx::Size widget_size,
      gfx::Rect anchor_bounds,
      std::optional<gfx::Rect> parent_bounds = std::nullopt);

  explicit ComposeDialogView(
      View* anchor_view,
      std::unique_ptr<WebUIContentsWrapperT<ComposeUntrustedUI>> bubble_wrapper,
      const gfx::Rect& anchor_bounds,
      views::BubbleBorder::Arrow anchor_position);

  ~ComposeDialogView() override;

  // WebUIBubbleDialogView:
  gfx::Rect GetBubbleBounds() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  WebUIContentsWrapperT<ComposeUntrustedUI>* bubble_wrapper() {
    return bubble_wrapper_.get();
  }

  base::WeakPtr<ComposeDialogView> GetWeakPtr();

 private:
  gfx::Rect anchor_bounds_;
  std::unique_ptr<WebUIContentsWrapperT<ComposeUntrustedUI>> bubble_wrapper_;
  base::WeakPtrFactory<ComposeDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_VIEW_H_

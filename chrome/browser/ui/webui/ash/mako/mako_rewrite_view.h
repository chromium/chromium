// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_REWRITE_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_REWRITE_VIEW_H_

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// A view to contain the Mako rewrite UI.
class MakoRewriteView : public WebUIBubbleDialogView {
 public:
  METADATA_HEADER(MakoRewriteView);

  MakoRewriteView(BubbleContentsWrapper* contents_wrapper,
                  const gfx::Rect& caret_bounds);
  MakoRewriteView(const MakoRewriteView&) = delete;
  MakoRewriteView& operator=(const MakoRewriteView&) = delete;
  ~MakoRewriteView() override;

  // WebUIBubbleDialogView:
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

 private:
  gfx::Rect caret_bounds_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_REWRITE_VIEW_H_

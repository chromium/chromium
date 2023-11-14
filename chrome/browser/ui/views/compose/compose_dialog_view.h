// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_VIEW_H_

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/compose/compose_ui.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// A view for the contents area of the Compose dialog.
class ComposeDialogView : public WebUIBubbleDialogView {
 public:
  METADATA_HEADER(ComposeDialogView);

  explicit ComposeDialogView(
      View* anchor_view,
      std::unique_ptr<BubbleContentsWrapperT<ComposeUI>> bubble_wrapper,
      const gfx::Rect& anchor_bounds,
      views::BubbleBorder::Arrow anchor_position);

  ~ComposeDialogView() override;

  BubbleContentsWrapperT<ComposeUI>* bubble_wrapper() {
    return bubble_wrapper_.get();
  }

  base::WeakPtr<ComposeDialogView> GetWeakPtr();

 private:
  std::unique_ptr<BubbleContentsWrapperT<ComposeUI>> bubble_wrapper_;
  base::WeakPtrFactory<ComposeDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_VIEW_H_

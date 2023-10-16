// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_VIEW_H_

#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

namespace compose {

// A view for the contents area of the Compose dialog.
class ComposeDialogView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(ComposeDialogView);

  explicit ComposeDialogView(View* anchor_view,
                             views::BubbleBorder::Arrow anchor_position,
                             const gfx::Rect bounds,
                             content::WebContents* web_contents);

  ~ComposeDialogView() override;
};

}  // namespace compose

#endif  // CHROME_BROWSER_UI_VIEWS_COMPOSE_COMPOSE_DIALOG_VIEW_H_

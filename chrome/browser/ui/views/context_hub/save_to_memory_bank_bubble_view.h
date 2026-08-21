// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_SAVE_TO_MEMORY_BANK_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_SAVE_TO_MEMORY_BANK_BUBBLE_VIEW_H_

#include <string>

#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_tracker.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
class WebView;
}  // namespace views

class SaveToMemoryBankBubbleView : public views::BubbleDialogDelegate,
                                   public content::WebContentsDelegate {
 public:
  SaveToMemoryBankBubbleView(views::View* anchor_view, Profile* profile);
  SaveToMemoryBankBubbleView(const SaveToMemoryBankBubbleView&) = delete;
  SaveToMemoryBankBubbleView& operator=(const SaveToMemoryBankBubbleView&) =
      delete;
  ~SaveToMemoryBankBubbleView() override;

  // views::WidgetDelegate:
  std::u16string GetWindowTitle() const override;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;

  views::WebView* web_view_for_testing();

 private:
  void InitLayout(Profile* profile);

  views::ViewTracker web_view_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_SAVE_TO_MEMORY_BANK_BUBBLE_VIEW_H_

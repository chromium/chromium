// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_PALETTE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_PALETTE_BUBBLE_VIEW_H_

#include <string>

#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_tracker.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

// Bubble view that hosts chrome://context-hub/memory_banks_palette WebUI
// for browsing, searching, copying, and attaching Memory Bank entries.
class MemoryBanksPaletteBubbleView : public views::BubbleDialogDelegate,
                                     public content::WebContentsDelegate {
 public:
  MemoryBanksPaletteBubbleView(views::BubbleAnchor anchor, Profile* profile);
  MemoryBanksPaletteBubbleView(const MemoryBanksPaletteBubbleView&) = delete;
  MemoryBanksPaletteBubbleView& operator=(const MemoryBanksPaletteBubbleView&) =
      delete;
  ~MemoryBanksPaletteBubbleView() override;

  // views::WidgetDelegate:
  std::u16string GetWindowTitle() const override;

  // content::WebContentsDelegate:
  void CloseContents(content::WebContents* source) override;

  views::WebView* web_view_for_testing();

 private:
  void InitLayout(Profile* profile);

  views::ViewTracker web_view_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_PALETTE_BUBBLE_VIEW_H_

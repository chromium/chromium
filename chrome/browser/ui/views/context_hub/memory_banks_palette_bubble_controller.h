// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_PALETTE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_PALETTE_BUBBLE_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/widget/widget.h"

class MemoryBanksPaletteBubbleView;

// Manages the lifetime and presentation of the Memory Banks Palette bubble.
class MemoryBanksPaletteBubbleController
    : public content::WebContentsUserData<MemoryBanksPaletteBubbleController>,
      public content::WebContentsObserver {
 public:
  MemoryBanksPaletteBubbleController(
      const MemoryBanksPaletteBubbleController&) = delete;
  MemoryBanksPaletteBubbleController& operator=(
      const MemoryBanksPaletteBubbleController&) = delete;
  ~MemoryBanksPaletteBubbleController() override;

  void ShowBubble();
  void CloseBubble();

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

 private:
  explicit MemoryBanksPaletteBubbleController(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<MemoryBanksPaletteBubbleController>;
  friend class MemoryBanksPaletteBubbleControllerTest;

  void ShowBubbleWithAnchor(views::BubbleAnchor anchor);
  void OnBubbleClosed(views::Widget::ClosedReason reason);
  void ResetBubble();

  // bubble_delegate_ must be declared before bubble_widget_ so
  // bubble_widget_ is destroyed first (BubbleDialogDelegate::CreateBubble
  // requires the delegate to outlive the widget).
  std::unique_ptr<MemoryBanksPaletteBubbleView> bubble_delegate_;
  std::unique_ptr<views::Widget> bubble_widget_;

  base::WeakPtrFactory<MemoryBanksPaletteBubbleController> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_MEMORY_BANKS_PALETTE_BUBBLE_CONTROLLER_H_

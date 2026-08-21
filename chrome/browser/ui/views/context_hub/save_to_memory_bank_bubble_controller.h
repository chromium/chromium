// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_SAVE_TO_MEMORY_BANK_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_SAVE_TO_MEMORY_BANK_BUBBLE_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/widget/widget.h"

class SaveToMemoryBankBubbleView;

// Manages the lifetime and presentation of the Save to Memory Bank bubble.
class SaveToMemoryBankBubbleController
    : public content::WebContentsUserData<SaveToMemoryBankBubbleController>,
      public content::WebContentsObserver {
 public:
  SaveToMemoryBankBubbleController(const SaveToMemoryBankBubbleController&) =
      delete;
  SaveToMemoryBankBubbleController& operator=(
      const SaveToMemoryBankBubbleController&) = delete;
  ~SaveToMemoryBankBubbleController() override;

  void ShowBubble();
  void CloseBubble();

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

 private:
  explicit SaveToMemoryBankBubbleController(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SaveToMemoryBankBubbleController>;

  void OnBubbleClosed(views::Widget::ClosedReason reason);

  std::unique_ptr<SaveToMemoryBankBubbleView> bubble_delegate_;
  std::unique_ptr<views::Widget> bubble_widget_;

  base::WeakPtrFactory<SaveToMemoryBankBubbleController> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXT_HUB_SAVE_TO_MEMORY_BANK_BUBBLE_CONTROLLER_H_

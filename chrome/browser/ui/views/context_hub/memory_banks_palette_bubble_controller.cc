// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/context_hub/memory_banks_palette_bubble_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/context_hub/memory_banks_palette_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(MemoryBanksPaletteBubbleController);

MemoryBanksPaletteBubbleController::MemoryBanksPaletteBubbleController(
    content::WebContents* web_contents)
    : content::WebContentsUserData<MemoryBanksPaletteBubbleController>(
          *web_contents),
      content::WebContentsObserver(web_contents) {}

MemoryBanksPaletteBubbleController::~MemoryBanksPaletteBubbleController() =
    default;

void MemoryBanksPaletteBubbleController::ShowBubble() {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(&GetWebContents());
  if (!tab) {
    return;
  }
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser) {
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }
  views::BubbleAnchor anchor(browser_view->GetContentsView());
  if (ToolbarButtonProvider* provider =
          browser_view->toolbar_button_provider()) {
    views::BubbleAnchor default_anchor =
        provider->GetDefaultExtensionDialogAnchor();
    if (!default_anchor.IsNull()) {
      anchor = default_anchor;
    }
  }

  ShowBubbleWithAnchor(anchor);
}

void MemoryBanksPaletteBubbleController::ShowBubbleWithAnchor(
    views::BubbleAnchor anchor) {
  if (bubble_widget_ && !bubble_widget_->IsClosed()) {
    bubble_widget_->Activate();
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  bubble_delegate_ =
      std::make_unique<MemoryBanksPaletteBubbleView>(anchor, profile);
  MemoryBanksPaletteBubbleView* delegate_ptr = bubble_delegate_.get();

  bubble_widget_ = views::BubbleDialogDelegate::CreateBubble(
      delegate_ptr,
      base::BindOnce(&MemoryBanksPaletteBubbleController::OnBubbleClosed,
                     weak_ptr_factory_.GetWeakPtr()));
  bubble_widget_->Show();
}

void MemoryBanksPaletteBubbleController::CloseBubble() {
  if (bubble_widget_ && !bubble_widget_->IsClosed()) {
    bubble_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void MemoryBanksPaletteBubbleController::PrimaryPageChanged(
    content::Page& page) {
  CloseBubble();
}

void MemoryBanksPaletteBubbleController::OnBubbleClosed(
    views::Widget::ClosedReason reason) {
  if (bubble_widget_) {
    bubble_widget_->Hide();
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MemoryBanksPaletteBubbleController::ResetBubble,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MemoryBanksPaletteBubbleController::ResetBubble() {
  bubble_widget_.reset();
  bubble_delegate_.reset();
}

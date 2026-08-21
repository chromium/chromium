// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/context_hub/save_to_memory_bank_bubble_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/context_hub/save_to_memory_bank_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveToMemoryBankBubbleController);

SaveToMemoryBankBubbleController::SaveToMemoryBankBubbleController(
    content::WebContents* web_contents)
    : content::WebContentsUserData<SaveToMemoryBankBubbleController>(
          *web_contents),
      content::WebContentsObserver(web_contents) {}

SaveToMemoryBankBubbleController::~SaveToMemoryBankBubbleController() = default;

void SaveToMemoryBankBubbleController::ShowBubble() {
  if (bubble_widget_ && !bubble_widget_->IsClosed()) {
    bubble_widget_->Activate();
    return;
  }

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
  views::View* anchor_view = browser_view->contents_web_view();
  if (browser_view->toolbar_button_provider()) {
    views::View* default_anchor = browser_view->toolbar_button_provider()
                                      ->GetDefaultExtensionDialogAnchor()
                                      .GetIfView();
    if (default_anchor) {
      anchor_view = default_anchor;
    }
  }

  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  bubble_delegate_ =
      std::make_unique<SaveToMemoryBankBubbleView>(anchor_view, profile);
  SaveToMemoryBankBubbleView* delegate_ptr = bubble_delegate_.get();

  bubble_widget_ = views::BubbleDialogDelegate::CreateBubble(
      delegate_ptr,
      base::BindOnce(&SaveToMemoryBankBubbleController::OnBubbleClosed,
                     weak_ptr_factory_.GetWeakPtr()));
  bubble_widget_->Show();
}

void SaveToMemoryBankBubbleController::CloseBubble() {
  if (bubble_widget_ && !bubble_widget_->IsClosed()) {
    bubble_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void SaveToMemoryBankBubbleController::PrimaryPageChanged(content::Page& page) {
  CloseBubble();
}

void SaveToMemoryBankBubbleController::OnBubbleClosed(
    views::Widget::ClosedReason reason) {
  if (bubble_widget_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(bubble_widget_));
  }
  if (bubble_delegate_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(bubble_delegate_));
  }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/autofill/core/browser/ui/payments/local_card_migration_bubble_controller.h"

namespace content {
class WebContents;
}

namespace autofill {

// Class responsible for showing the local card migration bubble which is
// the entry point of the entire migration flow.
class LocalCardMigrationBubbleViews : public LocalCardMigrationBubble,
                                      public LocationBarBubbleDelegateView {
 public:
  // The |controller| is lazily initialized in ChromeAutofillClient and there
  // should be only one controller per tab after the initialization. It should
  // live even when bubble is gone.
  LocalCardMigrationBubbleViews(views::View* anchor_view,
                                content::WebContents* web_contents,
                                LocalCardMigrationBubbleController* controller);

  void Show(DisplayReason reason);

  // LocalCardMigrationBubble:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;

 private:
  friend class LocalCardMigrationBrowserTest;

  ~LocalCardMigrationBubbleViews() override;

  // views::BubbleDialogDelegateView:
  void Init() override;

  LocalCardMigrationBubbleController* controller_;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationBubbleViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_VIEWS_H_

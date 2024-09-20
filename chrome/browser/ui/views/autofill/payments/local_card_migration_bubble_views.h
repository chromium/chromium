// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "components/autofill/core/browser/ui/payments/local_card_migration_bubble_controller.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}

namespace autofill {

// Class responsible for showing the local card migration bubble which is
// the entry point of the entire migration flow.
class LocalCardMigrationBubbleViews : public AutofillLocationBarBubble {
  METADATA_HEADER(LocalCardMigrationBubbleViews, AutofillLocationBarBubble)
 public:
  // The |controller| is lazily initialized in ChromeAutofillClient and there
  // should be only one controller per tab after the initialization. It should
  // live even when bubble is gone.
  LocalCardMigrationBubbleViews(views::View* anchor_view,
                                content::WebContents* web_contents,
                                LocalCardMigrationBubbleController* controller);

  LocalCardMigrationBubbleViews(const LocalCardMigrationBubbleViews&) = delete;
  LocalCardMigrationBubbleViews& operator=(
      const LocalCardMigrationBubbleViews&) = delete;
  ~LocalCardMigrationBubbleViews() override;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  friend class LocalCardMigrationBrowserTest;

  void OnDialogAccepted();
  void OnDialogCancelled();

  // views::BubbleDialogDelegateView:
  void Init() override;

  raw_ptr<LocalCardMigrationBubbleController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_VIEWS_H_

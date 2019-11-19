// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_VIEWS_H_

#include <memory>

#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_view.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/signin/public/base/signin_metrics.h"

namespace content {
class WebContents;
}

namespace autofill {

// This class serves as a base view to any of the bubble views that are part of
// the flow for when the user submits a form with a credit card number that
// Autofill has not previously saved. The base view establishes the button
// handlers, the calculated size, the Super G logo, testing methods, the
// SyncPromoDelegate and the window title (controller eventually handles the
// title for each sub-class).
class SaveCardBubbleViews : public SaveCardBubbleView,
                            public LocationBarBubbleDelegateView {
 public:
  // Bubble will be anchored to |anchor_view|.
  SaveCardBubbleViews(views::View* anchor_view,
                      content::WebContents* web_contents,
                      SaveCardBubbleController* controller);

  void Show(DisplayReason reason);

  // SaveCardBubbleView:
  void Hide() override;

  // views::BubbleDialogDelegateView:
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  // views::WidgetDelegate:
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;

  // Returns the footnote view, so it can be searched for clickable views.
  // Exists for testing (specifically, browsertests).
  views::View* GetFootnoteViewForTesting();

 protected:
  // Delegate for the personalized sync promo view used when desktop identity
  // consistency is enabled.
  class SyncPromoDelegate : public BubbleSyncPromoDelegate {
   public:
    SyncPromoDelegate(SaveCardBubbleController* controller,
                      signin_metrics::AccessPoint access_point);

    // BubbleSyncPromoDelegate:
    void OnEnableSync(const AccountInfo& account,
                      bool is_default_promo_account) override;

   private:
    SaveCardBubbleController* controller_;

    signin_metrics::AccessPoint access_point_;

    DISALLOW_COPY_AND_ASSIGN(SyncPromoDelegate);
  };

  // Create the dialog's content view containing everything except for the
  // footnote.
  virtual std::unique_ptr<views::View> CreateMainContentView();

  // Called by sub-classes to initialize |footnote_view_|.
  virtual void InitFootnoteView(views::View* footnote_view);

  SaveCardBubbleController* controller() const { return controller_; }

  // Attributes IDs to the dialog's DialogDelegate-supplied buttons.
  void AssignIdsToDialogButtons();

  // views::BubbleDialogDelegateView:
  void Init() override;

  std::unique_ptr<SyncPromoDelegate> sync_promo_delegate_;

  ~SaveCardBubbleViews() override;

 private:
  friend class SaveCardBubbleViewsFullFormBrowserTest;

  views::View* footnote_view_ = nullptr;

  SaveCardBubbleController* controller_;  // Weak reference.

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_VIEWS_H_

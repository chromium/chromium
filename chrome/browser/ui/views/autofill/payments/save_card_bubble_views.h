// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}

namespace autofill {

// This class serves as a base view to any of the bubble views that are part of
// the flow for when the user submits a form with a credit card number that
// Autofill has not previously saved. The base view establishes the button
// handlers, the calculated size, the Super G logo, testing methods, and the
// window title (controller eventually handles the title for each sub-class).
class SaveCardBubbleViews : public AutofillLocationBarBubble {
  METADATA_HEADER(SaveCardBubbleViews, AutofillLocationBarBubble)
 public:
  // Bubble will be anchored to |anchor_view|.
  SaveCardBubbleViews(views::View* anchor_view,
                      content::WebContents* web_contents,
                      SaveCardBubbleController* controller);

  SaveCardBubbleViews(const SaveCardBubbleViews&) = delete;
  SaveCardBubbleViews& operator=(const SaveCardBubbleViews&) = delete;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

  // Returns the footnote view, so it can be searched for clickable views.
  // Exists for testing (specifically, browsertests).
  views::View* GetFootnoteViewForTesting();

  const std::u16string GetCardIdentifierString() const;

 protected:
  // Create the dialog's content view containing everything except for the
  // footnote.
  virtual std::unique_ptr<views::View> CreateMainContentView();

  // Called by sub-classes to initialize |footnote_view_|.
  virtual void InitFootnoteView(views::View* footnote_view);

  SaveCardBubbleController* controller() const { return controller_; }

  // Attributes IDs to the dialog's DialogDelegate-supplied buttons.
  void AssignIdsToDialogButtons();

  // LocationBarBubbleDelegateView:
  void Init() override;

  void OnDialogAccepted();

  ~SaveCardBubbleViews() override;

 private:
  friend class SaveCardBubbleViewsFullFormBrowserTest;

  std::unique_ptr<views::View> GetCardIdentifierView();

  raw_ptr<views::View> footnote_view_ = nullptr;

  raw_ptr<SaveCardBubbleController> controller_;  // Weak reference.
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_VIEWS_H_

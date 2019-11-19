// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "components/autofill/core/browser/ui/payments/local_card_migration_dialog_controller.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

enum class LocalCardMigrationDialogState;
class LocalCardMigrationOfferView;

class LocalCardMigrationDialogView : public LocalCardMigrationDialog,
                                     public views::BubbleDialogDelegateView {
 public:
  LocalCardMigrationDialogView(LocalCardMigrationDialogController* controller,
                               content::WebContents* web_contents);
  ~LocalCardMigrationDialogView() override;

  // LocalCardMigrationDialog
  void ShowDialog() override;
  void CloseDialog() override;

  // views::BubbleDialogDelegateView
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;
  int GetDialogButtons() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;
  void WindowClosing() override;

  // Called by MigratableCardView when the user clicks the trash can button.
  // |guid| is the GUID of the credit card to be deleted.
  void DeleteCard(const std::string& guid);
  void UpdateLayout();

 private:
  friend class LocalCardMigrationBrowserTest;

  void ConstructView();

  base::string16 GetOkButtonLabel() const;
  base::string16 GetCancelButtonLabel() const;

  LocalCardMigrationDialogController* controller_;

  content::WebContents* web_contents_;

  // Pointer points to the LocalCardMigrationOfferView. Can be null when the
  // dialog is not in the 'offer' state.
  LocalCardMigrationOfferView* offer_view_ = nullptr;

  // The view containing a list of cards. It is the content of the scroll bar.
  // Owned by the LocalCardMigrationOfferView.
  views::View* card_list_view_;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationDialogView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_

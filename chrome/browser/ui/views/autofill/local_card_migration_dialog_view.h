// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class LocalCardMigrationDialogController;
class LocalCardMigrationOfferView;

class LocalCardMigrationDialogView : public LocalCardMigrationDialog,
                                     public views::ButtonListener,
                                     public views::DialogDelegateView,
                                     public views::WidgetObserver {
 public:
  LocalCardMigrationDialogView(LocalCardMigrationDialogController* controller,
                               content::WebContents* web_contents);
  ~LocalCardMigrationDialogView() override;

  // LocalCardMigrationDialog
  void ShowDialog() override;
  void CloseDialog() override;
  void OnMigrationFinished() override;

  // views::DialogDelegateView
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  void AddedToWidget() override;
  bool ShouldShowCloseButton() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override;

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  void Init();
  base::string16 GetOkButtonLabel() const;
  base::string16 GetCancelButtonLabel() const;

  LocalCardMigrationDialogController* controller_;

  content::WebContents* web_contents_;

  // Pointer points to the LocalCardMigrationOfferView. Can be null when the
  // dialog is not in the 'offer' state.
  LocalCardMigrationOfferView* offer_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationDialogView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_

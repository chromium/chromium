// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "components/autofill/core/browser/ui/payments/local_card_migration_dialog_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
  METADATA_HEADER(LocalCardMigrationDialogView, views::BubbleDialogDelegateView)

 public:
  explicit LocalCardMigrationDialogView(
      LocalCardMigrationDialogController* controller);
  LocalCardMigrationDialogView(const LocalCardMigrationDialogView&) = delete;
  LocalCardMigrationDialogView& operator=(const LocalCardMigrationDialogView&) =
      delete;
  ~LocalCardMigrationDialogView() override;

  // LocalCardMigrationDialog:
  void ShowDialog(content::WebContents& web_contents) override;
  void CloseDialog() override;

  // Called by MigratableCardView when the user clicks the trash can button.
  // |guid| is the GUID of the credit card to be deleted.
  void OnCardCheckboxToggled();
  void DeleteCard(const std::string& guid);
  void UpdateLayout();

 private:
  friend class LocalCardMigrationBrowserTest;

  void ConstructView();
  void OnDialogAccepted();
  void OnDialogCancelled();
  bool GetEnableOkButton() const;

  std::u16string GetOkButtonLabel() const;
  std::u16string GetCancelButtonLabel() const;

  raw_ptr<LocalCardMigrationDialogController> controller_;

  // Pointer points to the LocalCardMigrationOfferView. Can be null when the
  // dialog is not in the 'offer' state.
  raw_ptr<LocalCardMigrationOfferView> offer_view_ = nullptr;

  // The view containing a list of cards. It is the content of the scroll bar.
  // Owned by the LocalCardMigrationOfferView.
  raw_ptr<views::View> card_list_view_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_

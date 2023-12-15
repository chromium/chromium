// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_ERROR_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_ERROR_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "components/autofill/core/browser/ui/payments/local_card_migration_dialog_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class LocalCardMigrationErrorDialogView
    : public LocalCardMigrationDialog,
      public views::BubbleDialogDelegateView {
  METADATA_HEADER(LocalCardMigrationErrorDialogView,
                  views::BubbleDialogDelegateView)

 public:
  explicit LocalCardMigrationErrorDialogView(
      LocalCardMigrationDialogController* controller);
  LocalCardMigrationErrorDialogView(const LocalCardMigrationErrorDialogView&) =
      delete;
  LocalCardMigrationErrorDialogView& operator=(
      const LocalCardMigrationErrorDialogView&) = delete;
  ~LocalCardMigrationErrorDialogView() override;

  // LocalCardMigrationDialog:
  void ShowDialog(content::WebContents& web_contents) override;
  void CloseDialog() override;

  // views::BubbleDialogDelegateView:
  void Init() override;

 private:
  raw_ptr<LocalCardMigrationDialogController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_ERROR_DIALOG_VIEW_H_

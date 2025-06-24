// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/shared_tab_group_version_upgrade_modal.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/grit/branded_strings.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/versioning_message_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "url/gurl.h"

namespace tab_groups {

// Define a DialogModelDelegate to handle button actions.
class SharedTabGroupVersionDialogDelegate : public ui::DialogModelDelegate {
 public:
  explicit SharedTabGroupVersionDialogDelegate(Browser* browser)
      : browser_(browser) {}

  // Called when the "Update Chrome" button is clicked.
  void OnUpdateChromeClicked() {
    NavigateParams params(browser_, GURL("chrome://settings/help"),
                          ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }

 private:
  raw_ptr<Browser> browser_;
};

void ShowSharedTabGroupVersionUpgradeModal(
    Browser* browser,
    tab_groups::VersioningMessageController* versioning_message_controller,
    bool should_show) {
  if (!should_show) {
    return;
  }

  auto delegate =
      std::make_unique<SharedTabGroupVersionDialogDelegate>(browser);
  SharedTabGroupVersionDialogDelegate* delegate_ptr = delegate.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(delegate))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_COLLABORATION_CHROME_OUT_OF_DATE_ERROR_DIALOG_HEADER))
          .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
              IDS_COLLABORATION_CHROME_OUT_OF_DATE_ERROR_DIALOG_CONTINUE_BODY)))
          .AddCancelButton(base::DoNothing(),
                           ui::DialogModel::Button::Params().SetLabel(
                               l10n_util::GetStringUTF16(IDS_NOT_NOW)))
          .AddOkButton(
              base::BindOnce(
                  &SharedTabGroupVersionDialogDelegate::OnUpdateChromeClicked,
                  base::Unretained(delegate_ptr)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON)))
          .Build();

  chrome::ShowBrowserModal(browser, std::move(dialog_model));

  versioning_message_controller->OnMessageUiShown(
      tab_groups::VersioningMessageController::MessageType::
          VERSION_OUT_OF_DATE_INSTANT_MESSAGE);
}

void MaybeShowSharedTabGroupVersionUpgradeModal(Browser* browser) {
  // Only show on normal browser.
  if (!browser->is_type_normal()) {
    return;
  }

  tab_groups::TabGroupSyncService* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser->profile());
  if (!tab_group_sync_service) {
    return;
  }

  tab_groups::VersioningMessageController* versioning_message_controller =
      tab_group_sync_service->GetVersioningMessageController();
  if (!versioning_message_controller) {
    return;
  }

  versioning_message_controller->ShouldShowMessageUiAsync(
      tab_groups::VersioningMessageController::MessageType::
          VERSION_OUT_OF_DATE_INSTANT_MESSAGE,
      base::BindOnce(&ShowSharedTabGroupVersionUpgradeModal, browser,
                     versioning_message_controller));
}

}  // namespace tab_groups

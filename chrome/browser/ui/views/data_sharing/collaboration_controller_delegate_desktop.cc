// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/data_sharing/collaboration_controller_delegate_desktop.h"

#include "chrome/browser/collaboration/collaboration_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/views/data_sharing/account_card_view.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/collaboration/public/service_status.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

namespace {
struct DialogText {
  bool valid;
  const std::u16string title;
  const std::u16string body;
  const std::u16string ok_button_text;
};

DialogText GetPromptDialogTextFromStatus(
    const collaboration::ServiceStatus& status) {
  bool valid;
  int title_id = 0;
  int body_id = 0;
  int ok_button_text_id = 0;
  switch (status.signin_status) {
    case collaboration::SigninStatus::kNotSignedIn:
      title_id = IDS_DATA_SHARING_NEED_SIGN_IN;
      body_id = IDS_DATA_SHARING_NEED_SIGN_IN_BODY;
      ok_button_text_id = IDS_DATA_SHARING_NEED_SIGN_IN_CONTINUE_BUTTON;
      valid = true;
      break;
    case collaboration::SigninStatus::kSignedInPaused:
      title_id = IDS_DATA_SHARING_NEED_VERIFY_ACCOUNT;
      body_id = IDS_DATA_SHARING_NEED_VERIFY_ACCOUNT_BODY;
      ok_button_text_id = IDS_DATA_SHARING_NEED_VERIFY_ACCOUNT_BUTTON;
      valid = true;
      break;
    case collaboration::SigninStatus::kSignedIn:
      switch (status.sync_status) {
        case collaboration::SyncStatus::kNotSyncing:
          title_id = IDS_DATA_SHARING_NEED_SYNC;
          body_id = IDS_DATA_SHARING_NEED_SYNC_BODY;
          ok_button_text_id = IDS_DATA_SHARING_NEED_SIGN_IN_CONTINUE_BUTTON;
          valid = true;
          break;
        case collaboration::SyncStatus::kSyncWithoutTabGroup:
          title_id = IDS_DATA_SHARING_NEED_SYNC_TAB_GROUPS;
          body_id = IDS_DATA_SHARING_NEED_SYNC_TAB_GROUPS_BODY;
          ok_button_text_id = IDS_DATA_SHARING_SETTINGS;
          valid = true;
          break;
        default:
          valid = false;
          break;
      }
      break;
    default:
      valid = false;
      break;
  }

  if (valid) {
    return DialogText(valid, l10n_util::GetStringUTF16(title_id),
                      l10n_util::GetStringUTF16(body_id),
                      l10n_util::GetStringUTF16(ok_button_text_id));
  } else {
    return DialogText(valid);
  }
}

void ShowSignInAndSyncUi(Profile* profile) {
  signin_ui_util::EnableSyncFromSingleAccountPromo(
      profile, GetAccountInfoFromProfile(profile),
      signin_metrics::AccessPoint::kCollaborationTabGroup);
}

}  // namespace

CollaborationControllerDelegateDesktop::CollaborationControllerDelegateDesktop(
    Browser* browser)
    : browser_(browser),
      collaboration_service_(
          collaboration::CollaborationServiceFactory::GetForProfile(
              browser_->GetProfile())) {
  browser_list_observer_.Observe(BrowserList::GetInstance());
}

CollaborationControllerDelegateDesktop::
    ~CollaborationControllerDelegateDesktop() = default;

void CollaborationControllerDelegateDesktop::PrepareFlowUI(
    base::OnceCallback<void()> exit_callback,
    ResultCallback result) {
  exit_callback_ = std::move(exit_callback);
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void CollaborationControllerDelegateDesktop::ShowError(const ErrorInfo& error,
                                                       ResultCallback result) {
  if (!browser_) {
    return;
  }

  ShowErrorDialog();
  error_ui_callback_ = std::move(result);
}

void CollaborationControllerDelegateDesktop::Cancel(ResultCallback result) {
  MaybeCloseDialogs();
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void CollaborationControllerDelegateDesktop::ShowAuthenticationUi(
    ResultCallback result) {
  MaybeShowSignInOrSyncPromptDialog();
  authentication_ui_callback_ = std::move(result);
}

void CollaborationControllerDelegateDesktop::NotifySignInAndSyncStatusChange() {
  // No-Op for desktop.
}

void CollaborationControllerDelegateDesktop::ShowJoinDialog(
    const data_sharing::GroupToken& token,
    const data_sharing::SharedDataPreview& preview_data,
    ResultCallback result) {
  if (!browser_) {
    return;
  }
  auto* controller =
      DataSharingBubbleController::GetOrCreateForBrowser(browser_);
  controller->SetOnCloseCallback(base::BindOnce(
      &CollaborationControllerDelegateDesktop::OnJoinDialogClosing,
      weak_ptr_factory_.GetWeakPtr(), std::move(result)));
  controller->SetShowErrorDialogCallback(
      base::BindOnce(&CollaborationControllerDelegateDesktop::ShowErrorDialog,
                     weak_ptr_factory_.GetWeakPtr()));
  controller->Show(token);
}

void CollaborationControllerDelegateDesktop::ShowShareDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultWithGroupTokenCallback result) {
  if (!browser_) {
    return;
  }
  CHECK(std::holds_alternative<tab_groups::LocalTabGroupID>(either_id));
  DataSharingBubbleController::GetOrCreateForBrowser(browser_)->Show(
      std::get<tab_groups::LocalTabGroupID>(either_id));
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess,
                        std::nullopt);
}

void CollaborationControllerDelegateDesktop::OnUrlReadyToShare(
    const data_sharing::GroupId& group_id,
    const GURL& url,
    ResultCallback result) {}

void CollaborationControllerDelegateDesktop::ShowManageDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultCallback result) {
  if (!browser_) {
    return;
  }
  CHECK(std::holds_alternative<tab_groups::LocalTabGroupID>(either_id));
  DataSharingBubbleController::GetOrCreateForBrowser(browser_)->Show(
      std::get<tab_groups::LocalTabGroupID>(either_id));
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void CollaborationControllerDelegateDesktop::PromoteTabGroup(
    const data_sharing::GroupId& group_id,
    ResultCallback result) {
  if (!browser_) {
    return;
  }
  // Open tab group by group id.
  tab_groups::TabGroupSyncService* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser_->GetProfile());
  base::Uuid sync_id;

  for (const tab_groups::SavedTabGroup& group :
       tab_group_sync_service->GetAllGroups()) {
    if (!group.collaboration_id().has_value()) {
      continue;
    }
    if (group.collaboration_id().value().value() == group_id.value()) {
      sync_id = group.saved_guid();
      if (sync_id.is_valid()) {
        break;
      }
    }
  }

  if (!sync_id.is_valid()) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure);
    return;
  }
  tab_group_sync_service->OpenTabGroup(
      sync_id, std::make_unique<tab_groups::TabGroupActionContextDesktop>(
                   browser_, tab_groups::OpeningSource::kConnectOnGroupShare));
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void CollaborationControllerDelegateDesktop::PromoteCurrentScreen() {
  if (!browser_) {
    return;
  }

  // Focus on the current browser.
  browser_->window()->Activate();

  MaybeShowSignInOrSyncPromptDialog();
}

void CollaborationControllerDelegateDesktop::OnFlowFinished() {
  MaybeCloseDialogs();
}

collaboration::ServiceStatus
CollaborationControllerDelegateDesktop::GetServiceStatus() {
  return collaboration_service_->GetServiceStatus();
}

void CollaborationControllerDelegateDesktop::OnBrowserClosing(
    Browser* browser) {
  // When the current browser is closing, cancel the flow because we can't show
  // any UI on the current browser.
  if (browser_ == browser) {
    browser_ = nullptr;
    ExitFlow();
  }
}

void CollaborationControllerDelegateDesktop::OnJoinDialogClosing(
    ResultCallback result) {
  // Joins flow should end when the shared tab group is open after join
  // or cancel without joining.
  // TODO(crbug.org/380287432): Only cancel the flow if user doesn't join the
  // group.
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kCancel);
}

void CollaborationControllerDelegateDesktop::ShowErrorDialog() {
  if (error_dialog_widget_) {
    return;
  }

  // TODO(crbug.com/366057481): Show more detail errors based on ErrorInfo.
  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder()
          .SetTitle(l10n_util::GetStringUTF16(IDS_DATA_SHARING_SOMETHING_WRONG))
          .AddParagraph(ui::DialogModelLabel(
              l10n_util::GetStringUTF16(IDS_DATA_SHARING_SOMETHING_WRONG_BODY)))
          .AddOkButton(
              base::BindOnce(
                  &CollaborationControllerDelegateDesktop::OnErrorDialogOk,
                  weak_ptr_factory_.GetWeakPtr()),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(IDS_DATA_SHARING_GOT_IT))
                  .SetEnabled(true)
                  .SetId(kDataSharingErrorDialogOkButtonElementId))
          .Build();
  error_dialog_widget_ =
      chrome::ShowBrowserModal(browser_, std::move(dialog_model));
}

void CollaborationControllerDelegateDesktop::MaybeShowSignInAndSyncUi() {
  collaboration::ServiceStatus status = GetServiceStatus();
  if (!browser_) {
    return;
  }

  if (status.IsAuthenticationValid()) {
    return;
  }

  Profile* profile = browser_->profile();
  switch (status.signin_status) {
    case collaboration::SigninStatus::kNotSignedIn:
      ShowSignInAndSyncUi(profile);
      break;
    case collaboration::SigninStatus::kSignedInPaused:
      signin_ui_util::ShowReauthForAccount(
          profile, GetAccountInfoFromProfile(profile).email,
          signin_metrics::AccessPoint::kCollaborationTabGroup);
      break;
    case collaboration::SigninStatus::kSignedIn:
      switch (status.sync_status) {
        case collaboration::SyncStatus::kNotSyncing:
          ShowSignInAndSyncUi(profile);
          break;
        case collaboration::SyncStatus::kSyncWithoutTabGroup:
          chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
          break;
        case collaboration::SyncStatus::kSyncEnabled:
          NOTREACHED();
      }
      break;
  }
}

void CollaborationControllerDelegateDesktop::
    MaybeShowSignInOrSyncPromptDialog() {
  if (!browser_) {
    return;
  }

  if (prompt_dialog_widget_) {
    return;
  }

  // Show prompt UI based on signin and sync status.
  collaboration::ServiceStatus status = GetServiceStatus();
  if (status.IsAuthenticationValid()) {
    return;
  }

  DialogText dialog_text = GetPromptDialogTextFromStatus(status);
  if (dialog_text.valid) {
    std::unique_ptr<ui::DialogModel> dialog_model =
        ui::DialogModel::Builder()
            .SetTitle(dialog_text.title)
            .AddParagraph(ui::DialogModelLabel(dialog_text.body))
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            .SetBannerImage(
                ui::ImageModel::FromResourceId(IDR_SHARED_TAB_GROUPS_LIGHT),
                ui::ImageModel::FromResourceId(IDR_SHARED_TAB_GROUPS_DARK))
#endif
            .AddCancelButton(
                base::BindOnce(&CollaborationControllerDelegateDesktop::
                                   OnPromptDialogCancel,
                               weak_ptr_factory_.GetWeakPtr()),
                ui::DialogModel::Button::Params().SetEnabled(true).SetId(
                    kDataSharingSigninPromptDialogCancelButtonElementId))
            .AddOkButton(
                base::BindOnce(
                    &CollaborationControllerDelegateDesktop::OnPromptDialogOk,
                    weak_ptr_factory_.GetWeakPtr()),
                ui::DialogModel::Button::Params()
                    .SetLabel(dialog_text.ok_button_text)
                    .SetEnabled(true))
            .AddCustomField(
                std::make_unique<views::BubbleDialogModelHost::CustomView>(
                    std::make_unique<AccountCardView>(
                        GetAccountInfoFromProfile(browser_->profile())),
                    views::BubbleDialogModelHost::FieldType::kText))
            .Build();
    prompt_dialog_widget_ =
        chrome::ShowBrowserModal(browser_, std::move(dialog_model));
  }
}

void CollaborationControllerDelegateDesktop::OnPromptDialogOk() {
  prompt_dialog_widget_ = nullptr;
  if (authentication_ui_callback_) {
    std::move(authentication_ui_callback_)
        .Run(CollaborationControllerDelegate::Outcome::kSuccess);
  }
  MaybeShowSignInAndSyncUi();
}

void CollaborationControllerDelegateDesktop::OnPromptDialogCancel() {
  prompt_dialog_widget_ = nullptr;
  if (authentication_ui_callback_) {
    std::move(authentication_ui_callback_)
        .Run(CollaborationControllerDelegate::Outcome::kCancel);
  }
}

void CollaborationControllerDelegateDesktop::OnErrorDialogOk() {
  error_dialog_widget_ = nullptr;
  if (error_ui_callback_) {
    std::move(error_ui_callback_)
        .Run(CollaborationControllerDelegate::Outcome::kSuccess);
  }
}

void CollaborationControllerDelegateDesktop::MaybeCloseDialogs() {
  if (!browser_) {
    return;
  }

  if (prompt_dialog_widget_) {
    if (!prompt_dialog_widget_->IsClosed()) {
      prompt_dialog_widget_->Close();
    }
    prompt_dialog_widget_ = nullptr;
  }

  if (error_dialog_widget_) {
    if (!error_dialog_widget_->IsClosed()) {
      error_dialog_widget_->Close();
    }
    error_dialog_widget_ = nullptr;
  }
}

void CollaborationControllerDelegateDesktop::ExitFlow() {
  if (exit_callback_) {
    std::move(exit_callback_).Run();
  }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_change_ui_controller.h"

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

namespace {

using ToastOptions = PasswordChangeToast::ToastOptions;

constexpr base::TimeDelta kToastDisplayTime = base::Seconds(4);

// Creates dialog offering password change to the user. `with_privacy_notice`
// specifies whether an additional privacy paragraph should be displayed.
std::unique_ptr<ui::DialogModel> CreateOfferChangePasswordDialog(
    base::OnceClosure accept_callback,
    base::RepeatingClosure navigate_to_settings_callback,
    bool with_privacy_notice,
    std::u16string email) {
  ui::DialogModelLabel::TextReplacement link = ui::DialogModelLabel::CreateLink(
      with_privacy_notice
          ? IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_LINK_WITH_PRIVACY_NOTICE
          : IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_LINK_WITHOUT_PRIVACY_NOTICE,
      std::move(navigate_to_settings_callback));

  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetBannerImage(
      ui::ImageModel::FromResourceId(IDR_PASSWORD_CHANGE_WARNING),
      ui::ImageModel::FromResourceId(IDR_PASSWORD_CHANGE_WARNING_DARK));
  dialog_builder.SetIcon(
      ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon()));
  dialog_builder.SetTitle(l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_TITLE));
  dialog_builder.AddParagraph(ui::DialogModelLabel::CreateWithReplacements(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_DIALOG_DETAILS,
      {ui::DialogModelLabel::CreatePlainText(std::move(email)), link}));
  dialog_builder.AddCancelButton(base::DoNothing(),
                                 ui::DialogModel::Button::Params().SetLabel(
                                     l10n_util::GetStringUTF16(IDS_NO_THANKS)));
  dialog_builder.AddOkButton(
      std::move(accept_callback),
      ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CHANGE_PASSWORD)));
  if (with_privacy_notice) {
    dialog_builder.AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_PRIVACY_NOTICE)));
  }
  return dialog_builder.Build();
}

// Creates dialog for failed states of password change flow.
std::unique_ptr<ui::DialogModel> CreatePasswordChangeFailedDialog(
    base::OnceClosure accept_callback,
    bool use_error_image) {
  auto image_light = ui::ImageModel::FromResourceId(
      use_error_image ? IDR_PASSWORD_CHANGE_WARNING
                      : IDR_PASSWORD_CHANGE_NEUTRAL);
  auto image_dark = ui::ImageModel::FromResourceId(
      use_error_image ? IDR_PASSWORD_CHANGE_WARNING_DARK
                      : IDR_PASSWORD_CHANGE_NEUTRAL_DARK);
  return ui::DialogModel::Builder()
      .SetBannerImage(image_light, image_dark)
      .SetTitle(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_TITLE))
      .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_BODY)))
      .AddCancelButton(base::DoNothing(),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_CLOSE)))
      .AddOkButton(
          std::move(accept_callback),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_ACCEPT_BUTTON)))
      .Build();
}

// Creates dialog for `PasswordChangeDelegate::State::kOtpDetected`.
std::unique_ptr<ui::DialogModel> CreateOtpDetectedDialog(
    base::OnceClosure accept_callback) {
  return ui::DialogModel::Builder()
      .SetBannerImage(
          ui::ImageModel::FromResourceId(IDR_PASSWORD_CHANGE_NEUTRAL),
          ui::ImageModel::FromResourceId(IDR_PASSWORD_CHANGE_NEUTRAL_DARK))
      // TODO(crbug.com/417937595): Update strings once finalized by UXW.
      .SetTitle(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_OTP_DURING_PASSWORD_CHANGE_TITLE))
      .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_OTP_DURING_PASSWORD_CHANGE_BODY)))
      .AddCancelButton(base::DoNothing(),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_CANCEL)))
      .AddOkButton(std::move(accept_callback),
                   ui::DialogModel::Button::Params().SetLabel(
                       l10n_util::GetStringUTF16(IDS_CONTINUE)))
      .Build();
}

}  // namespace

PasswordChangeUIController::PasswordChangeUIController(
    PasswordChangeDelegate* password_change_delegate,
    tabs::TabInterface* tab_interface)
    : password_change_delegate_(password_change_delegate),
      tab_interface_(tab_interface) {}

PasswordChangeUIController::~PasswordChangeUIController() {
  CloseDialogWidget(views::Widget::ClosedReason::kUnspecified);
  CloseToastWidget(views::Widget::ClosedReason::kUnspecified);
}

void PasswordChangeUIController::UpdateState(
    PasswordChangeDelegate::State state) {
  std::variant<ToastOptions, std::unique_ptr<ui::DialogModel>> configuration =
      GetDialogOrToastConfiguration(state);

  if (std::holds_alternative<ToastOptions>(configuration)) {
    if (toast_view_) {
      toast_view_->UpdateLayout(
          std::move(std::get<ToastOptions>(configuration)));
      return;
    }
    ShowToast(std::move(std::get<ToastOptions>(configuration)));
    return;
  }

  // Close the toast before attempting to open any dialog.
  CloseToastWidget(views::Widget::ClosedReason::kUnspecified);
  ShowDialog(
      std::move(std::get<std::unique_ptr<ui::DialogModel>>(configuration)));
}

std::variant<ToastOptions, std::unique_ptr<ui::DialogModel>>
PasswordChangeUIController::GetDialogOrToastConfiguration(
    PasswordChangeDelegate::State state) {
  auto open_password_change_tab_callback =
      base::BindOnce(&PasswordChangeUIController::OpenPasswordChangeTab,
                     weak_ptr_factory_.GetWeakPtr());
  auto cancel_password_change_callback =
      base::BindOnce(&PasswordChangeUIController::CancelPasswordChange,
                     weak_ptr_factory_.GetWeakPtr());
  auto navigate_to_settings_callback = base::BindRepeating(
      &PasswordChangeUIController::NavigateToPasswordChangeSettings,
      base::Unretained(this));
  Profile* profile = Profile::FromBrowserContext(
      tab_interface_->GetContents()->GetBrowserContext());
  std::u16string email = base::UTF8ToUTF16(GetDisplayableAccountName(
      SyncServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile)));

  switch (state) {
    /* Dialogs */
    case PasswordChangeDelegate::State::kWaitingForAgreement:
      return CreateOfferChangePasswordDialog(
          base::BindOnce(&PasswordChangeDelegate::OnPrivacyNoticeAccepted,
                         password_change_delegate_->AsWeakPtr()),
          std::move(navigate_to_settings_callback),
          /*with_privacy_notice=*/true, std::move(email));
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
      return CreateOfferChangePasswordDialog(
          base::BindOnce(&PasswordChangeUIController::StartPasswordChangeFlow,
                         weak_ptr_factory_.GetWeakPtr()),
          std::move(navigate_to_settings_callback),
          /*with_privacy_notice=*/false, std::move(email));
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
      return CreatePasswordChangeFailedDialog(
          std::move(open_password_change_tab_callback),
          /*use_error_image=*/false);
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
      return CreatePasswordChangeFailedDialog(
          std::move(open_password_change_tab_callback),
          /*use_error_image=*/true);
    case PasswordChangeDelegate::State::kOtpDetected:
      return CreateOtpDetectedDialog(
          std::move(open_password_change_tab_callback));

    /* Toasts */
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
      return ToastOptions(
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OMNIBOX_SIGN_IN_CHECK),
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CANCEL),
          std::move(cancel_password_change_callback));
    case PasswordChangeDelegate::State::kChangingPassword:
      return ToastOptions(
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OMNIBOX_CHANGING_PASSWORD),
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CANCEL),
          std::move(cancel_password_change_callback));
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      return ToastOptions(
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGED_TITLE),
          vector_icons::kPasswordManagerIcon,
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_VIEW_DETAILS_BUTTON),
          base::BindOnce(&PasswordChangeUIController::ShowPasswordDetails,
                         weak_ptr_factory_.GetWeakPtr()),
          true);
    case PasswordChangeDelegate::State::kCanceled:
      return ToastOptions(
          l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_PASSWORD_UNCHANGED),
          vector_icons::kPasswordManagerIcon, std::nullopt);
  }
}

void PasswordChangeUIController::ShowToast(ToastOptions options) {
  CHECK(tab_interface_);
  auto toast_view = std::make_unique<PasswordChangeToast>(std::move(options));
  toast_view_ = toast_view.get();
  auto params = std::make_unique<tabs::TabDialogManager::Params>();
  params->close_on_navigate = false;
  params->close_on_detach = false;
  params->disable_input = false;

  toast_widget_ =
      tab_interface_->GetTabFeatures()
          ->tab_dialog_manager()
          ->CreateAndShowDialog(toast_view.release(), std::move(params));
  toast_widget_->MakeCloseSynchronous(
      base::BindOnce(&PasswordChangeUIController::CloseToastWidget,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeUIController::ShowDialog(
    std::unique_ptr<ui::DialogModel> dialog_model) {
  CHECK(tab_interface_);
  if (!tab_interface_->CanShowModalUI()) {
    return;
  }

  std::unique_ptr<views::BubbleDialogModelHost> model_host =
      views::BubbleDialogModelHost::CreateModal(std::move(dialog_model),
                                                ui::mojom::ModalType::kChild);
  // TODO(crbug.com/338254375): Remove once it is a default state.
  model_host->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  dialog_widget_ = tab_interface_->GetTabFeatures()
                       ->tab_dialog_manager()
                       ->CreateAndShowDialog(
                           model_host.release(),
                           std::make_unique<tabs::TabDialogManager::Params>());
  dialog_widget_->MakeCloseSynchronous(
      base::BindOnce(&PasswordChangeUIController::CloseDialogWidget,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeUIController::OpenPasswordChangeTab() {
  CHECK(password_change_delegate_);

  password_change_delegate_->OpenPasswordChangeTab();
  password_change_delegate_->Stop();
}

void PasswordChangeUIController::StartPasswordChangeFlow() {
  CHECK(password_change_delegate_);
  password_change_delegate_->StartPasswordChangeFlow();
}

void PasswordChangeUIController::ShowPasswordDetails() {
  // TODO(crbug.com/338254375): Open password changed successfully bubble when
  // applicable.
  NavigateToPasswordDetailsPage(
      chrome::FindBrowserWithTab(tab_interface_->GetContents()),
      base::UTF16ToUTF8(password_change_delegate_->GetDisplayOrigin()),
      password_manager::ManagePasswordsReferrer::kPasswordChangeInfoBubble);

  CHECK(password_change_delegate_);
  password_change_delegate_->Stop();
}

void PasswordChangeUIController::CancelPasswordChange() {
  CHECK(password_change_delegate_);
  password_change_delegate_->CancelPasswordChangeFlow();

  // Post delayed task to stop password change. This will destroy the
  // controller.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasswordChangeDelegate::Stop,
                     password_change_delegate_->AsWeakPtr()),
      kToastDisplayTime);
}

void PasswordChangeUIController::NavigateToPasswordChangeSettings() {
  ShowSingletonTabOverwritingNTP(
      Profile::FromBrowserContext(
          tab_interface_->GetContents()->GetBrowserContext()),
      GURL(chrome::kChromeUiPasswordChangeUrl),
      NavigateParams::IGNORE_AND_NAVIGATE);
}

void PasswordChangeUIController::CloseDialogWidget(
    views::Widget::ClosedReason reason) {
  dialog_widget_.reset();
}

void PasswordChangeUIController::CloseToastWidget(
    views::Widget::ClosedReason reason) {
  toast_view_ = nullptr;
  toast_widget_.reset();
}

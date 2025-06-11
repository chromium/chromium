// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_change_ui_controller.h"

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"
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

// Whether a dialog should be displayed for a given `state`.
bool ShouldDisplayDialog(PasswordChangeDelegate::State state) {
  switch (state) {
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
    case PasswordChangeDelegate::State::kWaitingForAgreement:
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
    case PasswordChangeDelegate::State::kOtpDetected:
      return true;
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
    case PasswordChangeDelegate::State::kChangingPassword:
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      return false;
  }
}

// Creates dialog offering password change to the user. `with_privacy_notice`
// specifies whether an additional privacy paragraph should be displayed.
std::unique_ptr<ui::DialogModel> CreateOfferChangePasswordDialog(
    base::OnceClosure accept_callback,
    bool with_privacy_notice) {
  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetBannerImage(
      ui::ImageModel::FromResourceId(IDR_PASSWORD_CHANGE_WARNING),
      ui::ImageModel::FromResourceId(IDR_PASSWORD_CHANGE_WARNING_DARK));
  dialog_builder.SetIcon(
      ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon()));
  dialog_builder.SetTitle(l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_BUBBLE_TITLE));
  dialog_builder.AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_BUBBLE_DETAILS)));
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

// Creates dialog for `state`.
std::unique_ptr<ui::DialogModel> CreateDialog(
    PasswordChangeDelegate::State state,
    base::OnceClosure accept_callback) {
  switch (state) {
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
      return CreateOfferChangePasswordDialog(std::move(accept_callback),
                                             /*with_privacy_notice=*/false);
    case PasswordChangeDelegate::State::kWaitingForAgreement:
      return CreateOfferChangePasswordDialog(std::move(accept_callback),
                                             /*with_privacy_notice=*/true);
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
      return CreatePasswordChangeFailedDialog(std::move(accept_callback),
                                              /*use_error_image=*/false);
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
      return CreatePasswordChangeFailedDialog(std::move(accept_callback),
                                              /*use_error_image=*/true);
    case PasswordChangeDelegate::State::kOtpDetected:
      return CreateOtpDetectedDialog(std::move(accept_callback));
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
    case PasswordChangeDelegate::State::kChangingPassword:
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      NOTREACHED();
  }
}

std::optional<PasswordChangeToast::ToastOptions> GetConfiguration(
    PasswordChangeDelegate::State state) {
  switch (state) {
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
      return PasswordChangeToast::ToastOptions(
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OMNIBOX_SIGN_IN_CHECK),
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CANCEL));
    case PasswordChangeDelegate::State::kChangingPassword:
      return PasswordChangeToast::ToastOptions(
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OMNIBOX_CHANGING_PASSWORD),
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CANCEL));
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      return PasswordChangeToast::ToastOptions(
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGED_TITLE),
          vector_icons::kPasswordManagerIcon,
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_VIEW_DETAILS_BUTTON),
          true);
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
    case PasswordChangeDelegate::State::kWaitingForAgreement:
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
    case PasswordChangeDelegate::State::kOtpDetected:
      return std::nullopt;
  }
}

}  // namespace

PasswordChangeUIController::PasswordChangeUIController(
    PasswordChangeDelegate* password_change_delegate,
    base::WeakPtr<content::WebContents> web_contents)
    : password_change_delegate_(password_change_delegate),
      web_contents_(web_contents) {}

PasswordChangeUIController::~PasswordChangeUIController() {
  CloseDialogWidget(views::Widget::ClosedReason::kUnspecified);
  CloseToastWidget(views::Widget::ClosedReason::kUnspecified);
}

void PasswordChangeUIController::UpdateState(
    PasswordChangeDelegate::State state) {
  if (state_ == state) {
    return;
  }

  state_ = state;

  if (auto configuration = GetConfiguration(state_)) {
    if (toast_view_) {
      toast_view_->UpdateLayout(std::move(configuration).value());
    } else {
      ShowToast(std::move(configuration).value());
    }
    return;
  }

  // If there is no toast configuration for a given state, just close the
  // toast.
  CloseToastWidget(views::Widget::ClosedReason::kUnspecified);

  // TODO(crbug.com/417389698): Handle other states.
  if (ShouldDisplayDialog(state_)) {
    tabs::TabInterface* tab_interface =
        tabs::TabInterface::MaybeGetFromContents(web_contents_.get());
    if (!tab_interface || !tab_interface->CanShowModalUI()) {
      return;
    }

    base::OnceClosure accept_callback = base::BindOnce(
        &PasswordChangeUIController::OnDialogAccepted, base::Unretained(this));
    std::unique_ptr<views::BubbleDialogModelHost> model_host =
        views::BubbleDialogModelHost::CreateModal(
            CreateDialog(state_, std::move(accept_callback)),
            ui::mojom::ModalType::kChild);
    // TODO(crbug.com/338254375): Remove once it is a default state.
    model_host->SetOwnershipOfNewWidget(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    dialog_widget_ =
        tab_interface->GetTabFeatures()
            ->tab_dialog_manager()
            ->CreateAndShowDialog(
                model_host.release(),
                std::make_unique<tabs::TabDialogManager::Params>());
    dialog_widget_->MakeCloseSynchronous(
        base::BindOnce(&PasswordChangeUIController::CloseDialogWidget,
                       base::Unretained(this)));
  }
}

void PasswordChangeUIController::OnDialogAccepted() {
  CHECK(password_change_delegate_);

  switch (state_) {
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
      password_change_delegate_->StartPasswordChangeFlow();
      return;
    case PasswordChangeDelegate::State::kWaitingForAgreement:
      password_change_delegate_->OnPrivacyNoticeAccepted();
      return;
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
    case PasswordChangeDelegate::State::kOtpDetected:
      password_change_delegate_->OpenPasswordChangeTab();
      password_change_delegate_->Stop();
      return;
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
    case PasswordChangeDelegate::State::kChangingPassword:
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      NOTREACHED();
  }
}

void PasswordChangeUIController::ShowToast(
    PasswordChangeToast::ToastOptions options) {
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents_.get());
  CHECK(tab_interface);
  auto toast_view = std::make_unique<PasswordChangeToast>(std::move(options));
  toast_view_ = toast_view.get();
  auto params = std::make_unique<tabs::TabDialogManager::Params>();
  params->close_on_navigate = false;
  params->close_on_detach = false;
  params->disable_input = false;

  toast_widget_ =
      tab_interface->GetTabFeatures()
          ->tab_dialog_manager()
          ->CreateAndShowDialog(toast_view.release(), std::move(params));
  toast_widget_->MakeCloseSynchronous(base::BindOnce(
      &PasswordChangeUIController::CloseToastWidget, base::Unretained(this)));
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

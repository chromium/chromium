// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_change_ui_controller.h"

#include "base/functional/callback.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

namespace {

// Whether a dialog should be displayed for a given `state`.
bool ShouldDisplayDialog(PasswordChangeDelegate::State state) {
  switch (state) {
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
    case PasswordChangeDelegate::State::kOtpDetected:
      return true;
    case PasswordChangeDelegate::State::kWaitingForAgreement:
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
    case PasswordChangeDelegate::State::kChangingPassword:
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      return false;
  }
}

// Creates dialog for `PasswordChangeDelegate::State::kOfferingPasswordChange`.
std::unique_ptr<ui::DialogModel> CreateOfferChangePasswordDialog(
    base::OnceClosure accept_callback) {
  return ui::DialogModel::Builder()
      .SetBannerImage(
          ui::ImageModel::FromResourceId(IDR_PASSWORD_CHANGE_WARNING),
          ui::ImageModel::FromResourceId(IDR_PASSWORD_CHANGE_WARNING_DARK))
      .SetIcon(
          ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon()))
      .SetTitle(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_BUBBLE_TITLE))
      .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_BUBBLE_DETAILS)))
      .AddCancelButton(base::DoNothing(),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_NO_THANKS)))
      .AddOkButton(
          std::move(accept_callback),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CHANGE_PASSWORD)))
      .Build();
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
      return CreateOfferChangePasswordDialog(std::move(accept_callback));
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
      return CreatePasswordChangeFailedDialog(std::move(accept_callback),
                                              /*use_error_image=*/false);
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
      return CreatePasswordChangeFailedDialog(std::move(accept_callback),
                                              /*use_error_image=*/true);
    case PasswordChangeDelegate::State::kOtpDetected:
      return CreateOtpDetectedDialog(std::move(accept_callback));
    case PasswordChangeDelegate::State::kWaitingForAgreement:
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
    case PasswordChangeDelegate::State::kChangingPassword:
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      NOTREACHED();
  }
}

}  // namespace

PasswordChangeUIController::PasswordChangeUIController(
    PasswordChangeDelegate* password_change_delegate,
    base::WeakPtr<content::WebContents> web_contents)
    : password_change_delegate_(password_change_delegate),
      web_contents_(web_contents) {}

PasswordChangeUIController::~PasswordChangeUIController() = default;

void PasswordChangeUIController::UpdateState(
    PasswordChangeDelegate::State state) {
  if (state_ == state) {
    return;
  }

  state_ = state;

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
    tab_interface->GetTabFeatures()
        ->tab_dialog_manager()
        ->CreateAndShowDialog(
            model_host.release(),
            std::make_unique<tabs::TabDialogManager::Params>())
        .release();
    return;
  }
}

void PasswordChangeUIController::OnDialogAccepted() {
  CHECK(password_change_delegate_);

  switch (state_) {
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
      password_change_delegate_->StartPasswordChangeFlow();
      return;
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
    case PasswordChangeDelegate::State::kOtpDetected:
      password_change_delegate_->OpenPasswordChangeTab();
      password_change_delegate_->Stop();
      return;
    case PasswordChangeDelegate::State::kWaitingForAgreement:
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
    case PasswordChangeDelegate::State::kChangingPassword:
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      NOTREACHED();
  }
}

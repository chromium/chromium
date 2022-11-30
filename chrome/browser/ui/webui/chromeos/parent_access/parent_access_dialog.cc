// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_dialog.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

constexpr int kDialogHeightDp = 526;
constexpr int kDialogWidthDp = 600;

}  // namespace

ParentAccessDialogProvider::ShowError ParentAccessDialogProvider::Show(
    parent_access_ui::mojom::ParentAccessParamsPtr params,
    ParentAccessDialog::Callback callback) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile->IsChild()) {
    return ParentAccessDialogProvider::ShowError::kNotAChildUser;
  }

  if (ParentAccessDialog::GetInstance()) {
    return ParentAccessDialogProvider::ShowError::kDialogAlreadyVisible;
  }

  DCHECK(ParentAccessDialog::GetInstance() == nullptr);
  // Note:  |dialog_|'s memory is freed when
  // SystemWebDialogDelegate::OnDialogClosed() is called.
  ParentAccessDialog* dialog =
      new ParentAccessDialog(std::move(params), std::move(callback));

  dialog->ShowSystemDialogForBrowserContext(profile);
  return ParentAccessDialogProvider::ShowError::kNone;
}

// static
ParentAccessDialog* ParentAccessDialog::GetInstance() {
  return static_cast<ParentAccessDialog*>(
      SystemWebDialogDelegate::FindInstance(chrome::kChromeUIParentAccessURL));
}

ui::ModalType ParentAccessDialog::GetDialogModalType() const {
  return ui::ModalType::MODAL_TYPE_SYSTEM;
}

void ParentAccessDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidthDp, kDialogHeightDp);
}

bool ParentAccessDialog::ShouldCloseDialogOnEscape() const {
  return true;
}

parent_access_ui::mojom::ParentAccessParamsPtr
ParentAccessDialog::CloneParentAccessParams() {
  return parent_access_params_->Clone();
}

void ParentAccessDialog::SetResultAndClose(
    std::unique_ptr<ParentAccessDialog::Result> result) {
  DCHECK(!result_);
  result_ = std::move(result);
  // This will trigger dialog destruction, which will in turn result in the
  // callback being called.
  Close();
}

parent_access_ui::mojom::ParentAccessParams*
ParentAccessDialog::GetParentAccessParamsForTest() {
  return parent_access_params_.get();
}

ParentAccessDialog::ParentAccessDialog(
    parent_access_ui::mojom::ParentAccessParamsPtr params,
    ParentAccessDialog::Callback callback)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIParentAccessURL),
                              /*title=*/std::u16string()),
      parent_access_params_(std::move(params)),
      callback_(std::move(callback)) {}

ParentAccessDialog::~ParentAccessDialog() {
  std::move(callback_).Run(
      result_ ? std::move(result_)
              /* default status is kCancelled */
              : std::make_unique<ParentAccessDialog::Result>());
}

}  // namespace chromeos

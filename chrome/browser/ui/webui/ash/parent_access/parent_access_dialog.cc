// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/webui_url_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_dimmer.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_metrics_utils.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"

namespace ash {

namespace {

constexpr int kDialogHeightDp = 526;
constexpr int kDialogWidthDp = 600;
constexpr float kDimmerOpacity = 0.7f;
}  // namespace

void RecordParentAccessWidgetShowDialogError(
    ParentAccessDialogProvider::ShowErrorType error_type,
    std::optional<parent_access_ui::mojom::ParentAccessParams::FlowType>
        flow_type) {
  base::UmaHistogramEnumeration(
      parent_access::GetHistogramTitleForFlowType(
          parent_access::kParentAccessWidgetShowDialogErrorHistogramBase,
          flow_type),
      error_type);

  // Always record metric for "all" flow type.
  base::UmaHistogramEnumeration(
      parent_access::GetHistogramTitleForFlowType(
          parent_access::kParentAccessWidgetShowDialogErrorHistogramBase,
          std::nullopt),
      error_type);
}

ParentAccessDialogProvider::ShowError ParentAccessDialogProvider::Show(
    parent_access_ui::mojom::ParentAccessParamsPtr params,
    ParentAccessDialog::Callback callback) {
  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const auto* primary_session =
      session_manager::SessionManager::Get()->GetPrimarySession();
  CHECK(primary_session);
  const user_manager::User& user =
      CHECK_DEREF(user_manager::UserManager::Get()->FindUser(
          primary_session->account_id()));
  if (!supervised_user::IsSubjectToParentalControls(
          CHECK_DEREF(user.GetProfilePrefs()))) {
    RecordParentAccessWidgetShowDialogError(
        ParentAccessDialogProvider::ShowErrorType::kNotAChildUser,
        params->flow_type);
    return ParentAccessDialogProvider::ShowError::kNotAChildUser;
  }

  if (ParentAccessDialog::GetInstance()) {
    RecordParentAccessWidgetShowDialogError(
        ParentAccessDialogProvider::ShowErrorType::kAlreadyVisible,
        params->flow_type);
    return ParentAccessDialogProvider::ShowError::kDialogAlreadyVisible;
  }

  CHECK(ParentAccessDialog::GetInstance() == nullptr,
        base::NotFatalUntil::M162);
  // Note:  |dialog|'s memory is freed when
  // SystemWebDialogDelegate::OnDialogClosed() is called.
  ParentAccessDialog* dialog =
      new ParentAccessDialog(std::move(params), std::move(callback));

  // Dimmer should be shown before the dialog.
  dialog->ShowDimmer();
  dialog->ShowSystemDialogForBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          primary_session->account_id()));

  return ParentAccessDialogProvider::ShowError::kNone;
}

// static
ParentAccessDialog* ParentAccessDialog::GetInstance() {
  return static_cast<ParentAccessDialog*>(
      SystemWebDialogDelegate::FindInstance(ash::kChromeUIParentAccessURL));
}

ui::mojom::ModalType ParentAccessDialog::GetDialogModalType() const {
  return ui::mojom::ModalType::kSystem;
}

void ParentAccessDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidthDp, kDialogHeightDp);
}

bool ParentAccessDialog::ShouldCloseDialogOnEscape() const {
  return true;
}

bool ParentAccessDialog::ShouldShowDialogTitle() const {
  return false;
}
// The close button is implemented in the WebUI itself.
bool ParentAccessDialog::ShouldShowCloseButton() const {
  return false;
}

parent_access_ui::mojom::ParentAccessParamsPtr
ParentAccessDialog::CloneParentAccessParams() {
  return parent_access_params_->Clone();
}

void ParentAccessDialog::SetApproved(const std::string& parent_access_token,
                                     const base::Time& expire_timestamp) {
  auto result = std::make_unique<ParentAccessDialog::Result>();
  result->status = ParentAccessDialog::Result::Status::kApproved;
  result->parent_access_token = parent_access_token;
  result->parent_access_token_expire_timestamp = expire_timestamp;
  CloseWithResult(std::move(result));
}

void ParentAccessDialog::SetDeclined() {
  auto result = std::make_unique<ParentAccessDialog::Result>();
  result->status = ParentAccessDialog::Result::Status::kDeclined;
  CloseWithResult(std::move(result));
}

void ParentAccessDialog::SetCanceled() {
  auto result = std::make_unique<ParentAccessDialog::Result>();
  result->status = ParentAccessDialog::Result::Status::kCanceled;
  CloseWithResult(std::move(result));
}

void ParentAccessDialog::SetDisabled() {
  auto result = std::make_unique<ParentAccessDialog::Result>();
  result->status = ParentAccessDialog::Result::Status::kDisabled;
  CloseWithResult(std::move(result));
}

void ParentAccessDialog::SetError() {
  auto result = std::make_unique<ParentAccessDialog::Result>();
  result->status = ParentAccessDialog::Result::Status::kError;
  // Don't close dialog on error state, as user will close the dialog manually
  // after seeing the error.
  result_ = std::move(result);
}

parent_access_ui::mojom::ParentAccessParams*
ParentAccessDialog::GetParentAccessParamsForTest() const {
  return parent_access_params_.get();
}

ParentAccessDialog::ParentAccessDialog(
    parent_access_ui::mojom::ParentAccessParamsPtr params,
    ParentAccessDialog::Callback callback)
    : SystemWebDialogDelegate(GURL(ash::kChromeUIParentAccessURL),
                              /*title=*/std::u16string()),
      parent_access_params_(std::move(params)),
      callback_(std::move(callback)) {}

void ParentAccessDialog::ShowDimmer() {
  CHECK_EQ(nullptr, dimmer_, base::NotFatalUntil::M162);
  dimmer_ = std::make_unique<WindowDimmer>(
      Shell::GetPrimaryRootWindow()->GetChildById(
          kShellWindowId_SystemModalContainer));
  dimmer_->SetDimOpacity(kDimmerOpacity);
  dimmer_->window()->Show();
}

ParentAccessDialog::~ParentAccessDialog() {
  std::move(callback_).Run(
      result_ ? std::move(result_)
              /* default status is kCanceled */
              : std::make_unique<ParentAccessDialog::Result>());
}

void ParentAccessDialog::CloseWithResult(
    std::unique_ptr<ParentAccessDialog::Result> result) {
  result_ = std::move(result);
  // This will trigger dialog destruction, which will in turn result in the
  // callback being called.
  Close();
}

}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace web_app {

namespace {

// A DialogDelegate that notifies callers when it closes.
// Accept/Cancel/Close callbacks could be used together to figure out when a
// dialog closes, but this provides a simpler single callback.
class OnCompleteDialogDelegate : public views::DialogDelegate {
 public:
  ~OnCompleteDialogDelegate() override {
    if (callback_) {
      std::move(callback_).Run();
    }
  }

  void SetCompleteCallback(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

 private:
  base::OnceClosure callback_;
};

}  // namespace

IsolatedWebAppInstallerViewController::IsolatedWebAppInstallerViewController(
    Profile* profile,
    WebAppProvider* web_app_provider,
    IsolatedWebAppInstallerModel* model)
    : profile_(profile),
      web_app_provider_(web_app_provider),
      model_(model),
      view_(nullptr),
      dialog_delegate_(nullptr) {}

IsolatedWebAppInstallerViewController::
    ~IsolatedWebAppInstallerViewController() = default;

void IsolatedWebAppInstallerViewController::Start() {
  // TODO(crbug.com/1479140): Check if Feature is enabled
  model_->SetStep(IsolatedWebAppInstallerModel::Step::kGetMetadata);
  OnModelChanged();

  installability_checker_ = InstallabilityChecker::CreateAndStart(
      profile_, web_app_provider_, model_->bundle_path(), this);
}

void IsolatedWebAppInstallerViewController::Show(base::OnceClosure callback) {
  CHECK(!callback_);
  callback_ = std::move(callback);

  auto view = std::make_unique<IsolatedWebAppInstallerView>(this);
  view_ = view.get();
  std::unique_ptr<views::DialogDelegate> dialog_delegate =
      CreateDialogDelegate(std::move(view));
  dialog_delegate_ = dialog_delegate.get();

  OnModelChanged();

  views::DialogDelegate::CreateDialogWidget(std::move(dialog_delegate),
                                            /*context=*/nullptr,
                                            /*parent=*/nullptr)
      ->Show();
}

void IsolatedWebAppInstallerViewController::SetViewForTesting(
    IsolatedWebAppInstallerView* view) {
  view_ = view;
}

void IsolatedWebAppInstallerViewController::OnProfileShutdown() {
  Close();
}

void IsolatedWebAppInstallerViewController::OnBundleInvalid(
    const std::string& error) {
  // TODO(crbug.com/1479140): Show "failed to verify" error message
  Close();
}

void IsolatedWebAppInstallerViewController::OnBundleInstallable(
    const SignedWebBundleMetadata& metadata) {
  model_->SetSignedWebBundleMetadata(metadata);
  model_->SetStep(IsolatedWebAppInstallerModel::Step::kConfirmInstall);
  OnModelChanged();
}

void IsolatedWebAppInstallerViewController::OnBundleUpdatable(
    const SignedWebBundleMetadata& metadata,
    const base::Version& installed_version) {
  // TODO(crbug.com/1479140): Handle updates
  Close();
}

void IsolatedWebAppInstallerViewController::OnBundleOutdated(
    const SignedWebBundleMetadata& metadata,
    const base::Version& installed_version) {
  // TODO(crbug.com/1479140): Show "outdated" error message
  Close();
}

void IsolatedWebAppInstallerViewController::OnSettingsLinkClicked() {
  // TODO(crbug.com/1479140): Implement
}

// static
bool IsolatedWebAppInstallerViewController::OnAcceptWrapper(
    base::WeakPtr<IsolatedWebAppInstallerViewController> controller) {
  if (controller) {
    return controller->OnAccept();
  }
  return true;
}

bool IsolatedWebAppInstallerViewController::OnAccept() {
  // TODO(crbug.com/1479140): Implement
  switch (model_->step()) {
    case IsolatedWebAppInstallerModel::Step::kGetMetadata:
      return true;

    default:
      NOTREACHED();
  }
  OnModelChanged();
  return false;
}

void IsolatedWebAppInstallerViewController::OnComplete() {
  view_ = nullptr;
  dialog_delegate_ = nullptr;
  std::move(callback_).Run();
}

void IsolatedWebAppInstallerViewController::Close() {
  if (dialog_delegate_) {
    dialog_delegate_->CancelDialog();
  }
}

void IsolatedWebAppInstallerViewController::OnModelChanged() {
  // TODO(crbug.com/1479140): Configure Install/Cancel buttons for all screens
  switch (model_->step()) {
    case IsolatedWebAppInstallerModel::Step::kDisabled:
      SetButtons(IDS_APP_CLOSE, /*accept_button_label_id=*/absl::nullopt);
      view_->ShowDisabledScreen();
      break;

    case IsolatedWebAppInstallerModel::Step::kGetMetadata:
      SetButtons(IDS_APP_CANCEL, /*accept_button_label_id=*/absl::nullopt);
      view_->ShowGetMetadataScreen();
      break;

    case IsolatedWebAppInstallerModel::Step::kConfirmInstall:
      view_->ShowMetadataScreen(model_->bundle_metadata());
      break;

    case IsolatedWebAppInstallerModel::Step::kInstall:
      view_->ShowInstallScreen(model_->bundle_metadata());
      break;

    case IsolatedWebAppInstallerModel::Step::kInstallSuccess:
      view_->ShowInstallSuccessScreen(model_->bundle_metadata());
      break;
  }
}

void IsolatedWebAppInstallerViewController::SetButtons(
    int close_button_label_id,
    absl::optional<int> accept_button_label_id) {
  if (!dialog_delegate_) {
    return;
  }

  int buttons = ui::DIALOG_BUTTON_CANCEL;
  dialog_delegate_->SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(close_button_label_id));
  if (accept_button_label_id.has_value()) {
    buttons |= ui::DIALOG_BUTTON_OK;
    dialog_delegate_->SetButtonLabel(
        ui::DIALOG_BUTTON_OK,
        l10n_util::GetStringUTF16(accept_button_label_id.value()));
  }
  dialog_delegate_->SetButtons(buttons);
}

std::unique_ptr<views::DialogDelegate>
IsolatedWebAppInstallerViewController::CreateDialogDelegate(
    std::unique_ptr<views::View> contents_view) {
  auto delegate = std::make_unique<OnCompleteDialogDelegate>();
  delegate->set_internal_name("Isolated Web App Installer");
  delegate->SetOwnedByWidget(true);
  delegate->SetContentsView(std::move(contents_view));
  delegate->SetModalType(ui::MODAL_TYPE_WINDOW);
  delegate->SetShowCloseButton(false);
  delegate->SetHasWindowSizeControls(false);
  delegate->set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  // TODO(crbug.com/1479140): Set the title of the dialog for Alt+Tab
  delegate->SetShowTitle(false);

  delegate->SetAcceptCallbackWithClose(base::BindRepeating(
      &IsolatedWebAppInstallerViewController::OnAcceptWrapper,
      weak_ptr_factory_.GetWeakPtr()));
  delegate->SetCompleteCallback(
      base::BindOnce(&IsolatedWebAppInstallerViewController::OnComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  return delegate;
}

}  // namespace web_app

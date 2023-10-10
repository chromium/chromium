// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/isolated_web_apps/isolated_web_app_installer_view_controller.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/ui/web_applications/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/web_applications/isolated_web_apps/isolated_web_app_installer_view.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ui_base_types.h"
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
    WebAppProvider* web_app_provider,
    IsolatedWebAppInstallerModel* model)
    : web_app_provider_(web_app_provider),
      model_(model),
      view_(nullptr),
      dialog_delegate_(nullptr) {}

IsolatedWebAppInstallerViewController::
    ~IsolatedWebAppInstallerViewController() = default;

void IsolatedWebAppInstallerViewController::Show(base::OnceClosure callback) {
  CHECK(!callback_);
  callback_ = std::move(callback);

  auto view = std::make_unique<IsolatedWebAppInstallerView>(this);
  view_ = view.get();
  std::unique_ptr<views::DialogDelegate> dialog_delegate =
      CreateDialogDelegate(std::move(view));
  dialog_delegate_ = dialog_delegate.get();
  views::DialogDelegate::CreateDialogWidget(std::move(dialog_delegate),
                                            /*context=*/nullptr,
                                            /*parent=*/nullptr)
      ->Show();

  // TODO(crbug.com/1479140): Switch to "not enabled" if the pref isn't set
  model_->SetStep(IsolatedWebAppInstallerModel::Step::kGetMetadata);

  UpdateView();
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
  UpdateView();
  return false;
}

void IsolatedWebAppInstallerViewController::OnComplete() {
  view_ = nullptr;
  dialog_delegate_ = nullptr;
  std::move(callback_).Run();
}

void IsolatedWebAppInstallerViewController::UpdateView() {
  // TODO(crbug.com/1479140): Configure Install/Cancel buttons for all screens
  switch (model_->step()) {
    case IsolatedWebAppInstallerModel::Step::kDisabled:
      view_->ShowDisabledScreen();
      break;

    case IsolatedWebAppInstallerModel::Step::kGetMetadata:
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
  // TODO(crbug.com/1479140): Set the title of the dialog for Alt+Tab

  delegate->SetAcceptCallbackWithClose(base::BindRepeating(
      &IsolatedWebAppInstallerViewController::OnAcceptWrapper,
      weak_ptr_factory_.GetWeakPtr()));
  delegate->SetCompleteCallback(
      base::BindOnce(&IsolatedWebAppInstallerViewController::OnComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  return delegate;
}

}  // namespace web_app

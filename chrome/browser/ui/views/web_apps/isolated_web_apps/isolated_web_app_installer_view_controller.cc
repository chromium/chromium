// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "ui/events/event_constants.h"
#else
#include "base/command_line.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "url/gurl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

struct IsolatedWebAppInstallerViewController::InstallabilityCheckedVisitor {
  explicit InstallabilityCheckedVisitor(
      IsolatedWebAppInstallerModel& model,
      IsolatedWebAppInstallerViewController& controller)
      : model_(model), controller_(controller) {}

  void operator()(InstallabilityChecker::BundleInvalid) {
    model_->SetDialogContent(IsolatedWebAppInstallerModel::DialogContent(
        /*is_error=*/true, IDS_IWA_INSTALLER_VERIFICATION_ERROR_TITLE,
        IDS_IWA_INSTALLER_VERIFICATION_ERROR_SUBTITLE));
    controller_->OnModelChanged();
  }

  void operator()(InstallabilityChecker::BundleInstallable installable) {
    model_->SetSignedWebBundleMetadata(installable.metadata);
    model_->SetStep(IsolatedWebAppInstallerModel::Step::kShowMetadata);
    controller_->OnModelChanged();
  }

  void operator()(InstallabilityChecker::BundleUpdatable) {
    // TODO(crbug.com/1479140): Handle updates
    controller_->Close();
  }

  void operator()(InstallabilityChecker::BundleOutdated) {
    // TODO(crbug.com/1479140): Show "outdated" error message
    controller_->Close();
  }

  void operator()(InstallabilityChecker::ProfileShutdown) {
    controller_->Close();
  }

 private:
  raw_ref<IsolatedWebAppInstallerModel> model_;
  raw_ref<IsolatedWebAppInstallerViewController> controller_;
};

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
      profile_, web_app_provider_, model_->bundle_path(),
      base::BindOnce(
          &IsolatedWebAppInstallerViewController::OnInstallabilityChecked,
          weak_ptr_factory_.GetWeakPtr()));
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

// static
bool IsolatedWebAppInstallerViewController::OnAcceptWrapper(
    base::WeakPtr<IsolatedWebAppInstallerViewController> controller) {
  if (controller) {
    return controller->OnAccept();
  }
  return true;
}

// Returns true if the dialog should be closed.
bool IsolatedWebAppInstallerViewController::OnAccept() {
  switch (model_->step()) {
    case IsolatedWebAppInstallerModel::Step::kShowMetadata: {
      IsolatedWebAppInstallerModel::LinkInfo learn_more_link = {
          IDS_IWA_INSTALLER_CONFIRM_LEARN_MORE,
          base::BindRepeating(&IsolatedWebAppInstallerViewController::
                                  OnShowMetadataLearnMoreClicked,
                              base::Unretained(this))};
      model_->SetDialogContent(IsolatedWebAppInstallerModel::DialogContent(
          /*is_error=*/false, IDS_IWA_INSTALLER_CONFIRM_TITLE,
          IDS_IWA_INSTALLER_CONFIRM_SUBTITLE, learn_more_link,
          IDS_IWA_INSTALLER_CONFIRM_CONTINUE));
      OnModelChanged();
      return false;
    }

    case IsolatedWebAppInstallerModel::Step::kInstallSuccess: {
      webapps::AppId app_id = model_->bundle_metadata().app_id();
#if BUILDFLAG(IS_CHROMEOS)
      apps::AppServiceProxyFactory::GetForProfile(profile_)->Launch(
          app_id, ui::EF_NONE, apps::LaunchSource::kFromInstaller,
          /*window_info=*/nullptr);
#else
      web_app_provider_->scheduler().LaunchApp(
          app_id, *base::CommandLine::ForCurrentProcess(),
          /*current_directory=*/base::FilePath(),
          /*url_handler_launch_url=*/absl::nullopt,
          /*protocol_handler_launch_url=*/absl::nullopt,
          /*file_launch_url=*/absl::nullopt, /*launch_files=*/{},
          base::DoNothing());
#endif  // BUILDFLAG(IS_CHROMEOS)
      return true;
    }

    default:
      NOTREACHED();
  }
  return true;
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

void IsolatedWebAppInstallerViewController::OnInstallabilityChecked(
    InstallabilityChecker::Result result) {
  absl::visit(InstallabilityCheckedVisitor(*model_, *this), result);
}

void IsolatedWebAppInstallerViewController::OnInstallComplete(
    base::expected<InstallIsolatedWebAppCommandSuccess,
                   InstallIsolatedWebAppCommandError> result) {
  if (result.has_value()) {
    model_->SetStep(IsolatedWebAppInstallerModel::Step::kInstallSuccess);
  } else {
    model_->SetDialogContent(IsolatedWebAppInstallerModel::DialogContent(
        /*is_error=*/true, IDS_IWA_INSTALLER_INSTALL_FAILED_TITLE,
        IDS_IWA_INSTALLER_INSTALL_FAILED_SUBTITLE,
        /*details_link=*/absl::nullopt,
        IDS_IWA_INSTALLER_INSTALL_FAILED_RETRY));
  }
  OnModelChanged();
}

void IsolatedWebAppInstallerViewController::OnShowMetadataLearnMoreClicked() {
  // TODO(crbug.com/1479140): Implement
}

void IsolatedWebAppInstallerViewController::OnSettingsLinkClicked() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kManageIsolatedWebAppsSubpagePath);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  DCHECK(service->IsAvailable<crosapi::mojom::UrlHandler>());

  GURL manage_isolated_web_apps_subpage_url =
      GURL(chrome::kChromeUIOSSettingsURL)
          .Resolve(
              chromeos::settings::mojom::kManageIsolatedWebAppsSubpagePath);
  service->GetRemote<crosapi::mojom::UrlHandler>()->OpenUrl(
      manage_isolated_web_apps_subpage_url);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void IsolatedWebAppInstallerViewController::OnManageProfilesLinkClicked() {
  // TODO(crbug.com/1479140): Implement
}

void IsolatedWebAppInstallerViewController::OnChildDialogCanceled() {
  // Currently all child dialogs should close the installer when closed.
  Close();
}

void IsolatedWebAppInstallerViewController::OnChildDialogAccepted() {
  switch (model_->step()) {
    case IsolatedWebAppInstallerModel::Step::kShowMetadata: {
      model_->SetStep(IsolatedWebAppInstallerModel::Step::kInstall);
      model_->SetDialogContent(absl::nullopt);
      OnModelChanged();

      const SignedWebBundleMetadata& metadata = model_->bundle_metadata();
      web_app_provider_->scheduler().InstallIsolatedWebApp(
          metadata.url_info(), metadata.location(), metadata.version(),
          /*optional_keep_alive=*/nullptr,
          /*optional_profile_keep_alive=*/nullptr,
          base::BindOnce(
              &IsolatedWebAppInstallerViewController::OnInstallComplete,
              weak_ptr_factory_.GetWeakPtr()));
      break;
    }

    case IsolatedWebAppInstallerModel::Step::kInstall:
      // A child dialog on the install screen means the installation failed.
      // Accepting the dialog corresponds to the Retry button.
      model_->SetDialogContent(absl::nullopt);
      Start();
      break;

    default:
      NOTREACHED();
  }
}

void IsolatedWebAppInstallerViewController::OnModelChanged() {
  if (!view_) {
    return;
  }

  switch (model_->step()) {
    case IsolatedWebAppInstallerModel::Step::kDisabled:
      IsolatedWebAppInstallerView::SetDialogButtons(
          dialog_delegate_, IDS_APP_CLOSE,
          /*accept_button_label_id=*/absl::nullopt);
      view_->ShowDisabledScreen();
      break;

    case IsolatedWebAppInstallerModel::Step::kGetMetadata:
      IsolatedWebAppInstallerView::SetDialogButtons(
          dialog_delegate_, IDS_APP_CANCEL,
          /*accept_button_label_id=*/absl::nullopt);
      view_->ShowGetMetadataScreen();
      break;

    case IsolatedWebAppInstallerModel::Step::kShowMetadata:
      IsolatedWebAppInstallerView::SetDialogButtons(
          dialog_delegate_, IDS_APP_CANCEL, IDS_INSTALL);
      view_->ShowMetadataScreen(model_->bundle_metadata());
      break;

    case IsolatedWebAppInstallerModel::Step::kInstall:
      IsolatedWebAppInstallerView::SetDialogButtons(
          dialog_delegate_, IDS_APP_CANCEL,
          /*accept_button_label_id=*/absl::nullopt);
      view_->ShowInstallScreen(model_->bundle_metadata());
      break;

    case IsolatedWebAppInstallerModel::Step::kInstallSuccess:
      IsolatedWebAppInstallerView::SetDialogButtons(
          dialog_delegate_, IDS_IWA_INSTALLER_SUCCESS_FINISH,
          IDS_IWA_INSTALLER_SUCCESS_LAUNCH_APPLICATION);
      view_->ShowInstallSuccessScreen(model_->bundle_metadata());
      break;
  }

  if (model_->has_dialog_content()) {
    view_->ShowDialog(model_->dialog_content());
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

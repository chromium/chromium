// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/callback_delayer.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/pref_observer.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
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
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/ui/ash/shelf/isolated_web_app_installer_shelf_item_controller.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/mojom/lacros_shelf_item_tracker.mojom.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "url/gurl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {

namespace {

constexpr base::TimeDelta kGetMetadataMinimumDelay = base::Seconds(2);
constexpr base::TimeDelta kInstallationMinimumDelay = base::Seconds(2);
constexpr double kProgressBarPausePercentage = 0.8;

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

std::string CreateRandomInstanceId() {
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  return uuid.AsLowercaseString();
}

}  // namespace

struct IsolatedWebAppInstallerViewController::InstallabilityCheckedVisitor {
  explicit InstallabilityCheckedVisitor(
      IsolatedWebAppInstallerModel& model,
      IsolatedWebAppInstallerViewController& controller)
      : model_(model), controller_(controller) {}

  void operator()(const InstallabilityChecker::BundleInvalid& invalid) {
    LOG(ERROR) << "Isolated Web App bundle installability check failed: "
               << invalid.error;
    model_->SetDialog(IsolatedWebAppInstallerModel::BundleInvalidDialog{});
  }

  void operator()(const InstallabilityChecker::BundleInstallable& installable) {
    if (!installable.metadata.icons().empty()) {
      // Get the last icon from |any|, size doesn't matter since Shelf will
      // rescale the icon anyway.
      controller_->SetIcon(gfx::ImageSkia::CreateFrom1xBitmap(
          installable.metadata.icons()
              .GetBitmapsForPurpose(IconPurpose::ANY)
              .rbegin()
              ->second));
      controller_->AddOrUpdateWindowToShelf();
    }
    model_->SetSignedWebBundleMetadata(installable.metadata);
    model_->SetStep(IsolatedWebAppInstallerModel::Step::kShowMetadata);
  }

  void operator()(const InstallabilityChecker::BundleUpdatable& updatable) {
    model_->SetDialog(
        IsolatedWebAppInstallerModel::BundleAlreadyInstalledDialog{
            updatable.metadata.app_name(), updatable.installed_version});
  }

  void operator()(const InstallabilityChecker::BundleOutdated& outdated) {
    // TODO(crbug.com/40280769): Once we have an update flow we should add
    // more specific error messages for newer vs same version already installed.
    model_->SetDialog(
        IsolatedWebAppInstallerModel::BundleAlreadyInstalledDialog{
            outdated.metadata.app_name(), outdated.installed_version});
  }

  void operator()(const InstallabilityChecker::ProfileShutdown&) {
    controller_->Close();
  }

 private:
  raw_ref<IsolatedWebAppInstallerModel> model_;
  raw_ref<IsolatedWebAppInstallerViewController> controller_;
};

IsolatedWebAppInstallerViewController::IsolatedWebAppInstallerViewController(
    Profile* profile,
    WebAppProvider* web_app_provider,
    IsolatedWebAppInstallerModel* model,
    std::unique_ptr<IsolatedWebAppsEnabledPrefObserver> pref_observer)
    : instance_id_(CreateRandomInstanceId()),
      profile_(profile),
      web_app_provider_(web_app_provider),
      model_(model),
      view_(nullptr),
      dialog_delegate_(nullptr),
      pref_observer_(std::move(pref_observer)) {
  CHECK(profile_);
  CHECK(model_);
  CHECK(web_app_provider_);
  CHECK(pref_observer_);
  model_->AddObserver(this);
}

IsolatedWebAppInstallerViewController::
    ~IsolatedWebAppInstallerViewController() {
  model_->RemoveObserver(this);
}

void IsolatedWebAppInstallerViewController::Start(
    base::OnceClosure initialized_callback,
    base::OnceClosure completion_callback) {
  CHECK(initialized_callback);
  initialized_callback_ = std::move(initialized_callback);

  CHECK(completion_callback);
  completion_callback_ = std::move(completion_callback);

  // This callback will be posted asynchronously by the |pref_observer_|:
  // - Once on `Start()` of `pref_observer_`.
  // - Every time the pref value is changed.
  IsolatedWebAppsEnabledPrefObserver::PrefChangedCallback
      pref_changed_callback = base::BindRepeating(
          &IsolatedWebAppInstallerViewController::OnPrefChanged,
          weak_ptr_factory_.GetWeakPtr());

  pref_observer_->Start(pref_changed_callback);
}

void IsolatedWebAppInstallerViewController::AddOrUpdateWindowToShelf() {
  if (!window_) {
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::LacrosShelfItemTracker>()) {
    std::string window_id =
        lacros_window_utility::GetRootWindowUniqueId(window_);

    crosapi::mojom::WindowDataPtr window_data =
        crosapi::mojom::WindowData::New();
    window_data->item_id = instance_id_;
    window_data->window_id = window_id;
    window_data->instance_type =
        crosapi::mojom::InstanceType::kIsolatedWebAppInstaller;
    window_data->icon = icon_;

    lacros_service->GetRemote<crosapi::mojom::LacrosShelfItemTracker>()
        ->AddOrUpdateWindow(std::move(window_data));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ShelfModel* shelf_model = ash::ShelfModel::Get();
  ash::ShelfID shelf_id = ash::ShelfID(instance_id_);

  int index = ash::ShelfModel::Get()->ItemIndexByID(shelf_id);

  if (index == -1) {
    // If there is no existing item by the ID in the Shelf, we add a new item.
    auto delegate =
        std::make_unique<IsolatedWebAppInstallerShelfItemController>(shelf_id);

    ash::ShelfItem item;
    item.id = shelf_id;
    item.title = delegate->GetTitle();
    CHECK(!item.title.empty());
    item.status = ash::STATUS_RUNNING;
    item.type = ash::TYPE_APP;
    if (!icon_.isNull()) {
      item.image = icon_;
    } else {
      item.image = IsolatedWebAppInstallerShelfItemController::
          GetDefaultInstallerShelfIcon();
    }

    shelf_model->Add(item, std::move(delegate));
  } else {
    // If the item already exists in the Shelf, we update.
    const ash::ShelfItem* existing_item = shelf_model->ItemByID(shelf_id);
    CHECK(existing_item);

    ash::ShelfItem item = *existing_item;
    // Icon is the only thing we update for now.
    if (!icon_.isNull()) {
      item.image = icon_;
    }

    ash::ShelfModel::Get()->Set(index, item);
  }

  static_cast<LacrosShelfItemController*>(
      shelf_model->GetShelfItemDelegate(shelf_id))
      ->AddWindow(window_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void IsolatedWebAppInstallerViewController::SetIcon(gfx::ImageSkia icon) {
  icon_ = icon;
}

void IsolatedWebAppInstallerViewController::SetViewForTesting(
    IsolatedWebAppInstallerView* view) {
  view_ = view;
}

views::Widget* IsolatedWebAppInstallerViewController::GetWidgetForTesting() {
  return widget_;
}

views::Widget*
IsolatedWebAppInstallerViewController::GetChildWidgetForTesting() {
  return child_widget_;
}

void IsolatedWebAppInstallerViewController::Show() {
  CHECK(is_initialized_) << "Show() is being called before initialized.";
  CHECK(!view_) << "Show() should not be called twice";

  auto view = IsolatedWebAppInstallerView::Create(this);
  view_ = view.get();

  std::unique_ptr<views::DialogDelegate> dialog_delegate =
      CreateDialogDelegate(std::move(view));
  dialog_delegate_ = dialog_delegate.get();

  OnStepChanged();
  OnChildDialogChanged();

  widget_ =
      views::DialogDelegate::CreateDialogWidget(std::move(dialog_delegate),
                                                /*context=*/nullptr,
                                                /*parent=*/nullptr);

  CHECK(!window_);
  window_ = widget_->GetNativeWindow();
  AddOrUpdateWindowToShelf();

  widget_->Show();
}

void IsolatedWebAppInstallerViewController::FocusWindow() {
  if (!window_) {
    return;
  }

  auto* widget = views::Widget::GetWidgetForNativeWindow(window_);
  widget->Activate();
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
      model_->SetDialog(IsolatedWebAppInstallerModel::ConfirmInstallationDialog{
          base::BindRepeating(&IsolatedWebAppInstallerViewController::
                                  OnShowMetadataLearnMoreClicked,
                              base::Unretained(this))});
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
          /*url_handler_launch_url=*/std::nullopt,
          /*protocol_handler_launch_url=*/std::nullopt,
          /*file_launch_url=*/std::nullopt, /*launch_files=*/{},
          base::DoNothing());
#endif  // BUILDFLAG(IS_CHROMEOS)
      return true;
    }

    default:
      NOTREACHED_IN_MIGRATION();
  }
  return true;
}

void IsolatedWebAppInstallerViewController::OnComplete() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ShelfModel* shelf_model = ash::ShelfModel::Get();
  ash::ShelfID shelf_id = ash::ShelfID(instance_id_);
  int index = shelf_model->ItemIndexByID(shelf_id);
  if (-1 != index) {
    shelf_model->RemoveItemAt(index);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  view_ = nullptr;
  dialog_delegate_ = nullptr;
  widget_ = nullptr;
  std::move(completion_callback_).Run();
}

void IsolatedWebAppInstallerViewController::Close() {
  if (dialog_delegate_) {
    dialog_delegate_->CancelDialog();
  }
}

void IsolatedWebAppInstallerViewController::OnPrefChanged(bool enabled) {
  if (enabled) {
    model_->SetStep(IsolatedWebAppInstallerModel::Step::kGetMetadata);
    model_->SetDialog(std::nullopt);
    if (!installability_checker_) {
      callback_delayer_ = std::make_unique<CallbackDelayer>(
          kGetMetadataMinimumDelay, kProgressBarPausePercentage,
          base::BindRepeating(&IsolatedWebAppInstallerViewController::
                                  OnGetMetadataProgressUpdated,
                              weak_ptr_factory_.GetWeakPtr()));
      installability_checker_ = InstallabilityChecker::CreateAndStart(
          profile_, web_app_provider_, model_->source(),
          callback_delayer_->StartDelayingCallback(base::BindOnce(
              &IsolatedWebAppInstallerViewController::OnInstallabilityChecked,
              weak_ptr_factory_.GetWeakPtr())));
    }
  } else {
    // Disables the installer if the user has not started installation yet.
    // If IWA is disabled after step::kInstall, we allow installation to
    // complete and blocks the IWA from launching.
    if (model_->step() < IsolatedWebAppInstallerModel::Step::kInstall) {
      model_->SetStep(IsolatedWebAppInstallerModel::Step::kDisabled);
      model_->SetDialog(std::nullopt);
      installability_checker_.reset();
    }
  }
  if (!is_initialized_) {
    is_initialized_ = true;
    std::move(initialized_callback_).Run();
  }
}

void IsolatedWebAppInstallerViewController::OnGetMetadataProgressUpdated(
    double progress) {
  if (view_) {
    view_->UpdateGetMetadataProgress(progress);
  }
}

void IsolatedWebAppInstallerViewController::OnInstallabilityChecked(
    InstallabilityChecker::Result result) {
  absl::visit(InstallabilityCheckedVisitor(*model_, *this), result);
}

void IsolatedWebAppInstallerViewController::OnInstallProgressUpdated(
    double progress) {
  if (view_) {
    view_->UpdateInstallProgress(progress);
  }
}

void IsolatedWebAppInstallerViewController::OnInstallComplete(
    base::expected<InstallIsolatedWebAppCommandSuccess,
                   InstallIsolatedWebAppCommandError> result) {
  if (result.has_value()) {
    model_->SetStep(IsolatedWebAppInstallerModel::Step::kInstallSuccess);
  } else {
    model_->SetDialog(IsolatedWebAppInstallerModel::InstallationFailedDialog{});
  }
}

void IsolatedWebAppInstallerViewController::OnShowMetadataLearnMoreClicked() {
  // TODO(crbug.com/40280769): Implement
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

void IsolatedWebAppInstallerViewController::OnChildDialogCanceled() {
  // Currently all child dialogs should close the installer when closed.
  Close();
}

void IsolatedWebAppInstallerViewController::OnChildDialogAccepted() {
  model_->SetDialog(std::nullopt);
  switch (model_->step()) {
    case IsolatedWebAppInstallerModel::Step::kShowMetadata: {
      model_->SetStep(IsolatedWebAppInstallerModel::Step::kInstall);

      callback_delayer_ = std::make_unique<CallbackDelayer>(
          kInstallationMinimumDelay, kProgressBarPausePercentage,
          base::BindRepeating(
              &IsolatedWebAppInstallerViewController::OnInstallProgressUpdated,
              weak_ptr_factory_.GetWeakPtr()));
      const SignedWebBundleMetadata& metadata = model_->bundle_metadata();
      web_app_provider_->scheduler().InstallIsolatedWebApp(
          metadata.url_info(),
          IsolatedWebAppInstallSource::FromGraphicalInstaller(
              model_->source().WithFileOp(IwaSourceBundleProdFileOp::kCopy,
                                          IwaSourceBundleDevFileOp::kCopy)),
          metadata.version(),
          /*optional_keep_alive=*/nullptr,
          /*optional_profile_keep_alive=*/nullptr,
          callback_delayer_->StartDelayingCallback(base::BindOnce(
              &IsolatedWebAppInstallerViewController::OnInstallComplete,
              weak_ptr_factory_.GetWeakPtr())));
      break;
    }

    case IsolatedWebAppInstallerModel::Step::kInstall:
      // A child dialog on the install screen means the installation failed.
      // Accepting the dialog corresponds to the Retry button.
      installability_checker_.reset();
      pref_observer_->Reset();
      Start(base::DoNothing(), std::move(completion_callback_));
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void IsolatedWebAppInstallerViewController::OnChildDialogDestroying() {
  child_widget_ = nullptr;
}

void IsolatedWebAppInstallerViewController::OnStepChanged() {
  if (!view_) {
    return;
  }

  switch (model_->step()) {
    case IsolatedWebAppInstallerModel::Step::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
    case IsolatedWebAppInstallerModel::Step::kDisabled:
      IsolatedWebAppInstallerView::SetDialogButtons(
          dialog_delegate_, IDS_APP_CLOSE,
          /*accept_button_label_id=*/std::nullopt);
      view_->ShowDisabledScreen();
      break;

    case IsolatedWebAppInstallerModel::Step::kGetMetadata:
      IsolatedWebAppInstallerView::SetDialogButtons(
          dialog_delegate_, IDS_APP_CANCEL,
          /*accept_button_label_id=*/std::nullopt);
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
          /*accept_button_label_id=*/std::nullopt);
      view_->ShowInstallScreen(model_->bundle_metadata());
      break;

    case IsolatedWebAppInstallerModel::Step::kInstallSuccess:
      IsolatedWebAppInstallerView::SetDialogButtons(
          dialog_delegate_, IDS_IWA_INSTALLER_SUCCESS_FINISH,
          IDS_IWA_INSTALLER_SUCCESS_LAUNCH_APPLICATION);
      view_->ShowInstallSuccessScreen(model_->bundle_metadata());
      break;
  }
}

void IsolatedWebAppInstallerViewController::OnChildDialogChanged() {
  if (model_->has_dialog()) {
    child_widget_ = view_->ShowDialog(model_->dialog());
  }
}

std::unique_ptr<views::DialogDelegate>
IsolatedWebAppInstallerViewController::CreateDialogDelegate(
    std::unique_ptr<views::View> contents_view) {
  gfx::Size contents_max_size = contents_view->GetMaximumSize();
  auto delegate = std::make_unique<OnCompleteDialogDelegate>();
  delegate->set_internal_name(
      IsolatedWebAppInstallerView::kInstallerWidgetName);
  delegate->SetOwnedByWidget(true);
  delegate->SetContentsView(std::move(contents_view));
  delegate->SetModalType(ui::mojom::ModalType::kWindow);
  delegate->SetShowCloseButton(false);
  delegate->SetHasWindowSizeControls(false);
  delegate->SetCanResize(false);
  delegate->set_fixed_width(contents_max_size.width());
  // TODO(crbug.com/40280769): Set the title of the dialog for Alt+Tab
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

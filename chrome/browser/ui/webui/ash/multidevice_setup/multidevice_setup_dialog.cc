// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/webui/common/trusted_types_util.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/oobe_dialog_size_utils.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_handler.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_localized_strings_provider.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/multidevice_setup_resources.h"
#include "chrome/grit/multidevice_setup_resources_map.h"
#include "chromeos/ash/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/url_provider.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/wm/core/shadow_types.h"

namespace ash::multidevice_setup {

// static
MultiDeviceSetupDialog* MultiDeviceSetupDialog::current_instance_ = nullptr;

// static
gfx::NativeWindow MultiDeviceSetupDialog::containing_window_ =
    gfx::NativeWindow();

// static
void MultiDeviceSetupDialog::Show() {
  // Focus the window hosting the dialog that has already been created.
  if (containing_window_) {
    DCHECK(current_instance_);
    containing_window_->Focus();
    return;
  }

  current_instance_ = new MultiDeviceSetupDialog();
  current_instance_->ShowSystemDialogForBrowserContext(
      ProfileManager::GetActiveUserProfile(), nullptr);

  containing_window_ = current_instance_->dialog_window();

  // Remove the black backdrop behind the dialog window which appears in tablet
  // and full-screen mode.
  WindowBackdrop::Get(containing_window_)
      ->SetBackdropMode(WindowBackdrop::BackdropMode::kDisabled);
}

// static
MultiDeviceSetupDialog* MultiDeviceSetupDialog::Get() {
  return current_instance_;
}

// static
void MultiDeviceSetupDialog::SetInstanceForTesting(
    MultiDeviceSetupDialog* instance) {
  current_instance_ = instance;
}

void MultiDeviceSetupDialog::AddOnCloseCallback(base::OnceClosure callback) {
  on_close_callbacks_.push_back(std::move(callback));
}

MultiDeviceSetupDialog::MultiDeviceSetupDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIMultiDeviceSetupUrl),
                              std::u16string()) {}

MultiDeviceSetupDialog::~MultiDeviceSetupDialog() {
  for (auto& callback : on_close_callbacks_)
    std::move(callback).Run();
}

void MultiDeviceSetupDialog::GetDialogSize(gfx::Size* size) const {
  const gfx::Size dialog_size = CalculateOobeDialogSizeForPrimaryDisplay();
  size->SetSize(dialog_size.width(), dialog_size.height());
}

void MultiDeviceSetupDialog::OnDialogClosed(const std::string& json_retval) {
  DCHECK(this == current_instance_);
  current_instance_ = nullptr;
  containing_window_ = nullptr;

  // Note: The call below deletes |this|, so there is no further need to keep
  // track of the pointer.
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

void MultiDeviceSetupDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->type = views::Widget::InitParams::Type::TYPE_WINDOW_FRAMELESS;
  params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params->shadow_elevation = wm::kShadowElevationActiveWindow;
}

MultiDeviceSetupDialogUI::MultiDeviceSetupDialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIMultiDeviceSetupHost);

  AddLocalizedStrings(source);
  source->UseStringsJs();

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kMultideviceSetupResources,
                      kMultideviceSetupResourcesSize),
      IDR_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_DIALOG_HTML);
  // Enabling trusted types via trusted_types_util must be done after
  // webui::SetupWebUIDataSource to override the trusted type CSP with correct
  // policies for JS WebUIs.
  ash::EnableTrustedTypesCSP(source);

  web_ui->AddMessageHandler(std::make_unique<MultideviceSetupHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
}

MultiDeviceSetupDialogUI::~MultiDeviceSetupDialogUI() = default;

void MultiDeviceSetupDialogUI::BindInterface(
    mojo::PendingReceiver<mojom::MultiDeviceSetup> receiver) {
  MultiDeviceSetupService* service =
      MultiDeviceSetupServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (service)
    service->BindMultiDeviceSetup(std::move(receiver));
}

void MultiDeviceSetupDialogUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MultiDeviceSetupDialogUI)

}  // namespace ash::multidevice_setup

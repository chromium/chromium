// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/webui_url_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/webui/common/trusted_types_util.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/oobe_dialog_size_utils.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_handler.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_localized_strings_provider.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/grit/multidevice_setup_resources.h"
#include "chrome/grit/multidevice_setup_resources_map.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/url_provider.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/webui/webui_util.h"
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
    CHECK(current_instance_, base::NotFatalUntil::M162);
    containing_window_->Focus();
    return;
  }

  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const auto* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  CHECK(active_session);
  current_instance_ = new MultiDeviceSetupDialog();
  current_instance_->ShowSystemDialogForBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          active_session->account_id()),
      nullptr);

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
    : SystemWebDialogDelegate(GURL(ash::kChromeUIMultiDeviceSetupURL),
                              std::u16string()) {}

MultiDeviceSetupDialog::~MultiDeviceSetupDialog() {
  for (auto& callback : on_close_callbacks_) {
    std::move(callback).Run();
  }
}

void MultiDeviceSetupDialog::GetDialogSize(gfx::Size* size) const {
  const gfx::Size dialog_size = CalculateOobeDialogSizeForPrimaryDisplay();
  size->SetSize(dialog_size.width(), dialog_size.height());
}

void MultiDeviceSetupDialog::OnDialogClosed(const std::string& json_retval) {
  CHECK(this == current_instance_, base::NotFatalUntil::M162);
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

bool MultiDeviceSetupDialogUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::IsUserBrowserContext(browser_context);
}

MultiDeviceSetupDialogUI::MultiDeviceSetupDialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, ash::kChromeUIMultiDeviceSetupHost);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  AddLocalizedStrings(source);
  source->UseStringsJs();

  webui::SetupWebUIDataSource(
      source, kMultideviceSetupResources,
      IDR_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_DIALOG_HTML);
  // Enabling trusted types via trusted_types_util must be done after
  // webui::SetupWebUIDataSource to override the trusted type CSP with correct
  // policies for JS WebUIs.
  ash::EnableTrustedTypesCSP(source);

  web_ui->AddMessageHandler(std::make_unique<MultideviceSetupHandler>(
      BrowserContextHelper::Get()->GetUserByBrowserContext(
          web_ui->GetWebContents()->GetBrowserContext())));
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
}

MultiDeviceSetupDialogUI::~MultiDeviceSetupDialogUI() = default;

void MultiDeviceSetupDialogUI::BindInterface(
    mojo::PendingReceiver<mojom::MultiDeviceSetup> receiver) {
  MultiDeviceSetupService* service =
      MultiDeviceSetupServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (service) {
    service->BindMultiDeviceSetup(std::move(receiver));
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(MultiDeviceSetupDialogUI)

}  // namespace ash::multidevice_setup

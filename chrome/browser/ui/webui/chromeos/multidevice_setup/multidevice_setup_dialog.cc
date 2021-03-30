// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/login/ui/oobe_dialog_size_utils.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_handler.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_localized_strings_provider.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/multidevice_setup_resources.h"
#include "chrome/grit/multidevice_setup_resources_map.h"
#include "chromeos/grit/chromeos_resources.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/cpp/url_provider.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

constexpr int kPreferredDialogHeightPx = 640;
constexpr int kPreferredDialogWidthPx = 768;

constexpr char kOobeDialogHeightParamKey[] = "dialog-height";
constexpr char kOobeDialogWidthParamKey[] = "dialog-width";

}  // namespace

// static
MultiDeviceSetupDialog* MultiDeviceSetupDialog::current_instance_ = nullptr;

// static
gfx::NativeWindow MultiDeviceSetupDialog::containing_window_ = nullptr;

// static
void MultiDeviceSetupDialog::Show() {
  // Focus the window hosting the dialog that has already been created.
  if (containing_window_) {
    DCHECK(current_instance_);
    containing_window_->Focus();
    return;
  }

  current_instance_ = new MultiDeviceSetupDialog();
  containing_window_ = chrome::ShowWebDialog(
      nullptr /* parent */, ProfileManager::GetActiveUserProfile(),
      current_instance_);

  // Remove the black backdrop behind the dialog window which appears in tablet
  // and full-screen mode.
  ash::WindowBackdrop::Get(containing_window_)
      ->SetBackdropMode(ash::WindowBackdrop::BackdropMode::kDisabled);
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
    : SystemWebDialogDelegate(CreateMultiDeviceSetupURL(), std::u16string()) {}

MultiDeviceSetupDialog::~MultiDeviceSetupDialog() {
  for (auto& callback : on_close_callbacks_)
    std::move(callback).Run();
}

GURL MultiDeviceSetupDialog::CreateMultiDeviceSetupURL() {
  GURL gurl(chrome::kChromeUIMultiDeviceSetupUrl);
  gfx::Size size;
  GetDialogSize(&size);
  gurl = net::AppendQueryParameter(gurl, kOobeDialogHeightParamKey,
                                   base::NumberToString(size.height()));
  gurl = net::AppendQueryParameter(gurl, kOobeDialogWidthParamKey,
                                   base::NumberToString(size.width()));
  return gurl;
}

void MultiDeviceSetupDialog::GetDialogSize(gfx::Size* size) const {
  if (features::IsNewOobeLayoutEnabled()) {
    const gfx::Size dialog_size = CalculateOobeDialogSizeForPrimrayDisplay();
    size->SetSize(dialog_size.width(), dialog_size.height());
  } else {
    // Note: The size is calculated once based on the current screen orientation
    // and is not ever updated. It might be possible to resize the dialog upon
    // each screen rotation, but https://crbug.com/1030993 prevents this from
    // working.
    // TODO(https://crbug.com/1030993): Explore resizing the dialog dynamically.
    static const gfx::Size dialog_size = ComputeDialogSizeForInternalScreen(
        gfx::Size(kPreferredDialogWidthPx, kPreferredDialogHeightPx));
    size->SetSize(dialog_size.width(), dialog_size.height());
  }
}

void MultiDeviceSetupDialog::OnDialogClosed(const std::string& json_retval) {
  DCHECK(this == current_instance_);
  current_instance_ = nullptr;
  containing_window_ = nullptr;

  // Note: The call below deletes |this|, so there is no further need to keep
  // track of the pointer.
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

MultiDeviceSetupDialogUI::MultiDeviceSetupDialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMultiDeviceSetupHost);

  source->DisableTrustedTypesCSP();

  chromeos::multidevice_setup::AddLocalizedStrings(source);
  source->UseStringsJs();

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kMultideviceSetupResources,
                      kMultideviceSetupResourcesSize),
      IDR_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_DIALOG_HTML);

  web_ui->AddMessageHandler(std::make_unique<MultideviceSetupHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

MultiDeviceSetupDialogUI::~MultiDeviceSetupDialogUI() = default;

void MultiDeviceSetupDialogUI::BindInterface(
    mojo::PendingReceiver<chromeos::multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  MultiDeviceSetupService* service =
      MultiDeviceSetupServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (service)
    service->BindMultiDeviceSetup(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MultiDeviceSetupDialogUI)

}  // namespace multidevice_setup

}  // namespace chromeos

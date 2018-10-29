// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_handler.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_localized_strings_provider.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/multidevice_setup_resources.h"
#include "chrome/grit/multidevice_setup_resources_map.h"
#include "chromeos/grit/chromeos_resources.h"
#include "chromeos/services/multidevice_setup/public/cpp/url_provider.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

constexpr int kDialogHeightPx = 640;
constexpr int kDialogWidthPx = 768;

}  // namespace

// static
MultiDeviceSetupDialog* MultiDeviceSetupDialog::current_instance_ = nullptr;

// static
void MultiDeviceSetupDialog::Show() {
  // The dialog is already showing, so there is nothing to do.
  if (current_instance_)
    return;

  current_instance_ = new MultiDeviceSetupDialog();

  // TODO(crbug.com/888629): In order to remove the X button on the top right of
  // of the dialog, passing |is_minimal_style| == true is required, but as of
  // now, that will prevent the dialog from presenting in full screen if tablet
  // mode is on. See bug for more details.
  chrome::ShowWebDialogInContainer(
      ash::kShellWindowId_DefaultContainer /* container_id */,
      ProfileManager::GetActiveUserProfile(), current_instance_,
      false /* is_minimal_style */);
}

// static
MultiDeviceSetupDialog* MultiDeviceSetupDialog::Get() {
  return current_instance_;
}

void MultiDeviceSetupDialog::AddOnCloseCallback(base::OnceClosure callback) {
  on_close_callbacks_.push_back(std::move(callback));
}

MultiDeviceSetupDialog::MultiDeviceSetupDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIMultiDeviceSetupUrl),
                              base::string16()) {}

MultiDeviceSetupDialog::~MultiDeviceSetupDialog() {
  for (auto& callback : on_close_callbacks_)
    std::move(callback).Run();
}

void MultiDeviceSetupDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidthPx, kDialogHeightPx);
}

void MultiDeviceSetupDialog::OnDialogClosed(const std::string& json_retval) {
  DCHECK(this == current_instance_);
  current_instance_ = nullptr;

  // Note: The call below deletes |this|, so there is no further need to keep
  // track of the pointer.
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

MultiDeviceSetupDialogUI::MultiDeviceSetupDialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMultiDeviceSetupHost);

  chromeos::multidevice_setup::AddLocalizedStrings(source);
  source->SetJsonPath("strings.js");
  source->SetDefaultResource(
      IDR_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_DIALOG_HTML);

  // Note: The |kMultiDeviceSetupResourcesSize| and |kMultideviceSetupResources|
  // fields are defined in the generated file
  // chrome/grit/multidevice_setup_resources_map.h.
  for (size_t i = 0; i < kMultideviceSetupResourcesSize; ++i) {
    source->AddResourcePath(kMultideviceSetupResources[i].name,
                            kMultideviceSetupResources[i].value);
  }

  web_ui->AddMessageHandler(std::make_unique<MultideviceSetupHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);

  // Add Mojo bindings to this WebUI so that Mojo calls can occur in JavaScript.
  AddHandlerToRegistry(base::BindRepeating(
      &MultiDeviceSetupDialogUI::BindMultiDeviceSetup, base::Unretained(this)));
}

MultiDeviceSetupDialogUI::~MultiDeviceSetupDialogUI() = default;

void MultiDeviceSetupDialogUI::BindMultiDeviceSetup(
    chromeos::multidevice_setup::mojom::MultiDeviceSetupRequest request) {
  service_manager::Connector* connector =
      content::BrowserContext::GetConnectorFor(
          web_ui()->GetWebContents()->GetBrowserContext());
  DCHECK(connector);

  connector->BindInterface(chromeos::multidevice_setup::mojom::kServiceName,
                           std::move(request));
}

}  // namespace multidevice_setup

}  // namespace chromeos

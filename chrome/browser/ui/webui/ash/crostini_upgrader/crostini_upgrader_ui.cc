// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_ui.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/crostini/crostini_upgrader.h"
#include "chrome/browser/ash/crostini/crostini_upgrader_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

void AddStringResources(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"upgrade", IDS_CROSTINI_UPGRADER_UPGRADE_BUTTON},
      {"retry", IDS_CROSTINI_UPGRADER_TRY_AGAIN_BUTTON},
      {"close", IDS_APP_CLOSE},
      {"cancel", IDS_APP_CANCEL},
      {"done", IDS_CROSTINI_UPGRADER_DONE_BUTTON},
      {"restore", IDS_CROSTINI_UPGRADER_RESTORE_BUTTON},
      {"notNow", IDS_CROSTINI_UPGRADER_NOT_NOW},

      {"promptTitle", IDS_CROSTINI_UPGRADER_TITLE},
      {"backingUpTitle", IDS_CROSTINI_UPGRADER_BACKING_UP_TITLE},
      {"backupErrorTitle", IDS_CROSTINI_UPGRADER_BACKUP_ERROR_TITLE},
      {"backupSucceededTitle", IDS_CROSTINI_UPGRADER_BACKUP_SUCCEEDED_TITLE},
      {"prechecksFailedTitle", IDS_CROSTINI_UPGRADER_PRECHECKS_FAILED_TITLE},
      {"upgradingTitle", IDS_CROSTINI_UPGRADER_UPGRADING_TITLE},
      {"restoreTitle", IDS_CROSTINI_UPGRADER_RESTORE_TITLE},
      {"restoreErrorTitle", IDS_CROSTINI_UPGRADER_RESTORE_ERROR_TITLE},
      {"restoreSucceededTitle", IDS_CROSTINI_UPGRADER_RESTORE_SUCCEEDED_TITLE},
      {"succeededTitle", IDS_CROSTINI_UPGRADER_SUCCEEDED_TITLE},
      {"cancelingTitle", IDS_CROSTINI_UPGRADER_CANCELING_TITLE},
      {"errorTitle", IDS_CROSTINI_UPGRADER_ERROR_TITLE},

      {"precheckNoNetwork", IDS_CROSTINI_UPGRADER_PRECHECKS_FAILED_NETWORK},
      {"precheckNoPower", IDS_CROSTINI_UPGRADER_PRECHECKS_FAILED_POWER},

      {"backingUpMessage", IDS_CROSTINI_UPGRADER_BACKING_UP_MESSAGE},
      {"backupErrorMessage", IDS_CROSTINI_UPGRADER_BACKUP_ERROR_MESSAGE},
      {"upgradingMessage", IDS_CROSTINI_UPGRADER_UPGRADING},
      {"restoreMessage", IDS_CROSTINI_UPGRADER_RESTORE_MESSAGE},
      {"restoreErrorMessage", IDS_CROSTINI_UPGRADER_RESTORE_ERROR_MESSAGE},
      {"logFileMessageError", IDS_CROSTINI_UPGRADER_LOG_FILE_ERROR},
      {"logFileMessageSuccess", IDS_CROSTINI_UPGRADER_LOG_FILE_SUCCESS},

      {"backupCheckboxMessage", IDS_CROSTINI_UPGRADER_BACKUP_CHECKBOX_MESSAGE},
      {"backupChangeLocation", IDS_CROSTINI_UPGRADER_BACKUP_CHANGE_LOCATION},
  };
  source->AddLocalizedStrings(kStrings);

  std::u16string learn_more_url =
      base::ASCIIToUTF16(std::string{chrome::kLinuxAppsLearnMoreURL} +
                         "&b=" + base::SysInfo::GetLsbReleaseBoard());
  source->AddString(
      "promptMessage",
      l10n_util::GetStringFUTF8(IDS_CROSTINI_UPGRADER_BODY, learn_more_url));

  std::u16string device_name = ui::GetChromeOSDeviceName();
  source->AddString("offlineError",
                    l10n_util::GetStringFUTF8(
                        IDS_CROSTINI_INSTALLER_OFFLINE_ERROR, device_name));
}

CrostiniUpgraderUI::CrostiniUpgraderUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI{web_ui} {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUICrostiniUpgraderHost);
  webui::EnableTrustedTypesCSP(source);
  webui::SetJSModuleDefaults(source);
  AddStringResources(source);

  source->AddResourcePath("images/linux_illustration.png",
                          IDR_LINUX_ILLUSTRATION);
  source->AddResourcePath("images/success_illustration.svg",
                          IDR_LINUX_SUCCESS_ILLUSTRATION);
  source->AddResourcePath("images/error_illustration.png",
                          IDR_PLUGIN_VM_INSTALLER_ERROR);
  source->AddResourcePath("app.js", IDR_CROSTINI_UPGRADER_APP_JS);
  source->AddResourcePath("app.html.js", IDR_CROSTINI_UPGRADER_APP_HTML_JS);
  source->AddResourcePath("browser_proxy.js",
                          IDR_CROSTINI_UPGRADER_BROWSER_PROXY_JS);
  source->AddResourcePath("crostini_upgrader.mojom-lite.js",
                          IDR_CROSTINI_UPGRADER_MOJO_LITE_JS);
  source->AddResourcePath("images/crostini_icon.svg", IDR_CROSTINI_ICON);
  source->SetDefaultResource(IDR_CROSTINI_UPGRADER_INDEX_HTML);
}

CrostiniUpgraderUI::~CrostiniUpgraderUI() = default;

bool CrostiniUpgraderUI::RequestClosePage() {
  if (page_closed_ || !page_handler_) {
    return true;
  }

  page_handler_->RequestClosePage();
  return false;
}

void CrostiniUpgraderUI::BindInterface(
    mojo::PendingReceiver<crostini_upgrader::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void CrostiniUpgraderUI::CreatePageHandler(
    mojo::PendingRemote<crostini_upgrader::mojom::Page> pending_page,
    mojo::PendingReceiver<crostini_upgrader::mojom::PageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());

  page_handler_ = std::make_unique<CrostiniUpgraderPageHandler>(
      web_ui()->GetWebContents(),
      crostini::CrostiniUpgraderFactory::GetForProfile(
          Profile::FromWebUI(web_ui())),
      std::move(pending_page_handler), std::move(pending_page),
      // Using Unretained(this) because |page_handler_| will not out-live
      // |this|.
      base::BindOnce(&CrostiniUpgraderUI::OnPageClosed, base::Unretained(this)),
      std::move(launch_callback_));
}

void CrostiniUpgraderUI::OnPageClosed() {
  page_closed_ = true;
  // CloseDialog() is a no-op if we are not in a dialog (e.g. user
  // access the page using the URL directly, which is not supported).
  ui::MojoWebDialogUI::CloseDialog(base::Value::List());
}

WEB_UI_CONTROLLER_TYPE_IMPL(CrostiniUpgraderUI)

}  // namespace ash

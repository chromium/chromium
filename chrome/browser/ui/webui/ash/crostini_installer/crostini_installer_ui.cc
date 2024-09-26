// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer_ui.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/crostini/crostini_disk.h"
#include "chrome/browser/ash/crostini/crostini_installer.h"
#include "chrome/browser/ash/crostini/crostini_installer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/isolated_world_ids.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace {
void AddStringResources(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"next", IDS_CROSTINI_INSTALLER_NEXT_BUTTON},
      {"back", IDS_CROSTINI_INSTALLER_BACK_BUTTON},
      {"install", IDS_CROSTINI_INSTALLER_INSTALL_BUTTON},
      {"retry", IDS_CROSTINI_INSTALLER_RETRY_BUTTON},
      {"settings", IDS_CROSTINI_INSTALLER_SETTINGS_BUTTON},
      {"close", IDS_APP_CLOSE},
      {"cancel", IDS_APP_CANCEL},
      {"learnMore", IDS_LEARN_MORE},

      {"promptTitle", IDS_CROSTINI_INSTALLER_TITLE},
      {"installingTitle", IDS_CROSTINI_INSTALLER_INSTALLING},
      {"cancelingTitle", IDS_CROSTINI_INSTALLER_CANCELING_TITLE},
      {"errorTitle", IDS_CROSTINI_INSTALLER_ERROR_TITLE},
      {"needUpdateTitle", IDS_CROSTINI_INSTALLER_NEED_UPDATE_TITLE},

      {"loadTerminaError", IDS_CROSTINI_INSTALLER_LOAD_TERMINA_ERROR},
      {"needUpdateError", IDS_CROSTINI_INSTALLER_NEED_UPDATE_ERROR},
      {"createDiskImageError", IDS_CROSTINI_INSTALLER_CREATE_DISK_IMAGE_ERROR},
      {"startTerminaVmError", IDS_CROSTINI_INSTALLER_START_TERMINA_VM_ERROR},
      {"startLxdError", IDS_CROSTINI_INSTALLER_START_LXD_ERROR},
      {"startContainerError", IDS_CROSTINI_INSTALLER_START_CONTAINER_ERROR},
      {"configureContainerError",
       IDS_CROSTINI_INSTALLER_CONFIGURE_CONTAINER_ERROR},
      {"setupContainerError", IDS_CROSTINI_INSTALLER_SETUP_CONTAINER_ERROR},
      {"unknownError", IDS_CROSTINI_INSTALLER_UNKNOWN_ERROR},

      {"loadTerminaMessage", IDS_CROSTINI_INSTALLER_LOAD_TERMINA_MESSAGE},
      {"createDiskImageMessage",
       IDS_CROSTINI_INSTALLER_CREATE_DISK_IMAGE_MESSAGE},
      {"startTerminaVmMessage",
       IDS_CROSTINI_INSTALLER_START_TERMINA_VM_MESSAGE},
      {"startLxdMessage", IDS_CROSTINI_INSTALLER_START_LXD_MESSAGE},
      {"startContainerMessage", IDS_CROSTINI_INSTALLER_START_CONTAINER_MESSAGE},
      {"configureContainerMessage",
       IDS_CROSTINI_INSTALLER_CONFIGURE_CONTAINER_MESSAGE},
      {"setupContainerMessage", IDS_CROSTINI_INSTALLER_SETUP_CONTAINER_MESSAGE},
      {"cancelingMessage", IDS_CROSTINI_INSTALLER_CANCELING},

      {"configureMessage", IDS_CROSTINI_INSTALLER_CONFIGURE_MESSAGE},
      {"diskSizeSubtitle", IDS_CROSTINI_INSTALLER_DISK_SIZE_SUBTITLE},
      {"diskSizeHint", IDS_CROSTINI_INSTALLER_DISK_SIZE_HINT},
      {"insufficientDiskError", IDS_CROSTINI_INSTALLER_INSUFFICIENT_DISK_ERROR},
      {"usernameLabel", IDS_CROSTINI_INSTALLER_USERNAME_LABEL},
      {"usernameInvalidFirstCharacterError",
       IDS_CROSTINI_INSTALLER_USERNAME_INVALID_FIRST_CHARACTER_ERROR},
      {"usernameInvalidCharactersError",
       IDS_CROSTINI_INSTALLER_USERNAME_INVALID_CHARACTERS_ERROR},
      {"usernameNotAvailableError",
       IDS_CROSTINI_INSTALLER_USERNAME_NOT_AVAILABLE_ERROR},
      {"customDiskSizeLabel", IDS_CROSTINI_INSTALLER_CUSTOM_DISK_SIZE_LABEL},
  };
  source->AddLocalizedStrings(kStrings);

  std::u16string device_name = ui::GetChromeOSDeviceName();

  source->AddString("promptMessage",
                    l10n_util::GetStringFUTF8(
                        IDS_CROSTINI_INSTALLER_BODY,
                        ui::FormatBytesWithUnits(
                            crostini::disk::kDownloadSizeBytes,
                            ui::DATA_UNITS_MEBIBYTE, /*show_units=*/true)));
  source->AddString("learnMoreUrl",
                    std::string{chrome::kLinuxAppsLearnMoreURL} +
                        "&b=" + base::SysInfo::GetLsbReleaseBoard());

  source->AddString(
      "minimumFreeSpaceUnmetError",
      l10n_util::GetStringFUTF8(
          IDS_CROSTINI_INSTALLER_MINIMUM_FREE_SPACE_UNMET_ERROR,
          ui::FormatBytesWithUnits(crostini::disk::kMinimumDiskSizeBytes +
                                       crostini::disk::kDiskHeadroomBytes,
                                   ui::DATA_UNITS_GIBIBYTE,
                                   /*show_units=*/true)));
  source->AddString(
      "lowSpaceAvailableWarning",
      l10n_util::GetStringFUTF8(
          IDS_CROSTINI_INSTALLER_DISK_RESIZE_RECOMMENDED_WARNING,
          ui::FormatBytesWithUnits(crostini::disk::kRecommendedDiskSizeBytes,
                                   ui::DATA_UNITS_GIBIBYTE,
                                   /*show_units=*/true)));
  source->AddString(
      "recommendedDiskSizeLabel",
      l10n_util::GetStringFUTF8(
          IDS_CROSTINI_INSTALLER_RECOMMENDED_DISK_SIZE_LABEL,
          ui::FormatBytesWithUnits(crostini::disk::kRecommendedDiskSizeBytes,
                                   ui::DATA_UNITS_GIBIBYTE,
                                   /*show_units=*/true)));
  source->AddString("offlineError",
                    l10n_util::GetStringFUTF8(
                        IDS_CROSTINI_INSTALLER_OFFLINE_ERROR, device_name));
}
}  // namespace

namespace ash {

CrostiniInstallerUI::CrostiniInstallerUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI{web_ui} {
  auto* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUICrostiniInstallerHost);
  webui::EnableTrustedTypesCSP(source);
  webui::SetJSModuleDefaults(source);
  AddStringResources(source);
  source->AddString("defaultContainerUsername",
                    crostini::DefaultContainerUserNameForProfile(profile));

  source->AddResourcePath("app.js", IDR_CROSTINI_INSTALLER_APP_JS);
  source->AddResourcePath("app.html.js", IDR_CROSTINI_INSTALLER_APP_HTML_JS);
  source->AddResourcePath("browser_proxy.js",
                          IDR_CROSTINI_INSTALLER_BROWSER_PROXY_JS);
  source->AddResourcePath("crostini_installer.mojom-lite.js",
                          IDR_CROSTINI_INSTALLER_MOJO_LITE_JS);
  source->AddResourcePath("crostini_types.mojom-lite.js",
                          IDR_CROSTINI_INSTALLER_TYPES_MOJO_LITE_JS);
  source->AddResourcePath("images/linux_illustration.png",
                          IDR_LINUX_ILLUSTRATION);
  source->AddResourcePath("images/crostini_icon.svg", IDR_CROSTINI_ICON);
  source->SetDefaultResource(IDR_CROSTINI_INSTALLER_INDEX_HTML);
}

CrostiniInstallerUI::~CrostiniInstallerUI() = default;

bool CrostiniInstallerUI::RequestClosePage() {
  if (page_closed_ || !page_handler_) {
    return true;
  }

  page_handler_->RequestClosePage();
  return false;
}

void CrostiniInstallerUI::ClickInstallForTesting() {
  web_ui()->GetWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"const app = document.querySelector('crostini-installer-app');"
      // If flag CrostiniUsername or CrostiniDiskResizing is turned on, there
      // will be a "next" button and we should click it to go to the config page
      // before clicking "install" button.
      u"app.$$('#next:not([hidden])')?.click();"
      u"app.$.install.click();",
      base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
}

void CrostiniInstallerUI::BindInterface(
    mojo::PendingReceiver<crostini_installer::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void CrostiniInstallerUI::CreatePageHandler(
    mojo::PendingRemote<crostini_installer::mojom::Page> pending_page,
    mojo::PendingReceiver<crostini_installer::mojom::PageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());

  page_handler_ = std::make_unique<CrostiniInstallerPageHandler>(
      crostini::CrostiniInstallerFactory::GetForProfile(
          Profile::FromWebUI(web_ui())),
      std::move(pending_page_handler), std::move(pending_page),
      // Using Unretained(this) because |page_handler_| will not out-live
      // |this|.
      base::BindOnce(&CrostiniInstallerUI::OnPageClosed,
                     base::Unretained(this)));
}

void CrostiniInstallerUI::OnPageClosed() {
  page_closed_ = true;
  // CloseDialog() is a no-op if we are not in a dialog (e.g. user
  // access the page using the URL directly, which is not supported).
  ui::MojoWebDialogUI::CloseDialog(base::Value::List());
}

WEB_UI_CONTROLLER_TYPE_IMPL(CrostiniInstallerUI)

}  // namespace ash

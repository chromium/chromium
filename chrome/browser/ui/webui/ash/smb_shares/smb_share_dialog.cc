// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/smb_shares/smb_share_dialog.h"

#include "ash/webui/common/trusted_types_util.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/smb_client/smb_service.h"
#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_handler.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_shares_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash::smb_dialog {
namespace {

constexpr int kSmbShareDialogHeight = 570;

void AddSmbSharesStrings(content::WebUIDataSource* html_source) {
  // Add strings specific to smb_dialog.
  smb_dialog::AddLocalizedStrings(html_source);

  // Add additional strings that are not specific to smb_dialog.
  static const struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"addSmbShare", IDS_SETTINGS_DOWNLOADS_SMB_SHARES_ADD_SHARE},
      {"add", IDS_ADD},
      {"cancel", IDS_CANCEL},
  };
  for (const auto& entry : localized_strings) {
    html_source->AddLocalizedString(entry.name, entry.id);
  }
}

}  // namespace

// static
void SmbShareDialog::Show() {
  SmbShareDialog* dialog = new SmbShareDialog();
  dialog->ShowSystemDialog();
}

SmbShareDialog::SmbShareDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUISmbShareURL),
                              std::u16string() /* title */) {}

SmbShareDialog::~SmbShareDialog() = default;

void SmbShareDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(SystemWebDialogDelegate::kDialogWidth, kSmbShareDialogHeight);
}

SmbShareDialogUI::SmbShareDialogUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUISmbShareHost);
  ash::EnableTrustedTypesCSP(source);

  AddSmbSharesStrings(source);

  Profile* const profile = Profile::FromWebUI(web_ui);
  const smb_client::SmbService* const smb_service =
      smb_client::SmbServiceFactory::Get(profile);
  bool is_kerberos_enabled =
      smb_service && smb_service->IsKerberosEnabledViaPolicy();
  source->AddBoolean("isKerberosEnabled", is_kerberos_enabled);

  bool is_guest =
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
      user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession();
  source->AddBoolean("isGuest", is_guest);

  source->AddBoolean("isCrosComponentsEnabled",
                     chromeos::features::IsCrosComponentsEnabled());

  source->UseStringsJs();
  source->SetDefaultResource(IDR_SMB_SHARES_DIALOG_CONTAINER_HTML);
  source->AddResourcePath("smb_share_dialog.js", IDR_SMB_SHARES_DIALOG_JS);

  web_ui->AddMessageHandler(std::make_unique<SmbHandler>(
      Profile::FromWebUI(web_ui), base::DoNothing()));
}

SmbShareDialogUI::~SmbShareDialogUI() = default;

void SmbShareDialogUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

bool SmbShareDialog::ShouldShowCloseButton() const {
  return false;
}

WEB_UI_CONTROLLER_TYPE_IMPL(SmbShareDialogUI)

}  // namespace ash::smb_dialog

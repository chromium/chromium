// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/certificate_manager_dialog_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/certificate_manager_localized_strings_provider.h"
#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

namespace {

void AddCertificateManagerStrings(content::WebUIDataSource* html_source) {
  struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"cancel", IDS_CANCEL},
      {"close", IDS_CLOSE},
      {"edit", IDS_SETTINGS_EDIT},
      {"moreActions", IDS_SETTINGS_MORE_ACTIONS},
      {"ok", IDS_OK},
  };
  for (const auto& entry : localized_strings)
    html_source->AddLocalizedString(entry.name, entry.id);
  certificate_manager::AddLocalizedStrings(html_source);
}

}  // namespace

CertificateManagerDialogUI::CertificateManagerDialogUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUICertificateManagerHost);

  source->DisableTrustedTypesCSP();

  AddCertificateManagerStrings(source);
  source->AddBoolean(
      "isGuest",
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
          user_manager::UserManager::Get()->IsLoggedInAsPublicAccount());
  source->AddBoolean(
      "isKiosk", user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp());

  source->UseStringsJs();
  source->SetDefaultResource(IDR_CERT_MANAGER_DIALOG_HTML);
  source->DisableContentSecurityPolicy();

  web_ui->AddMessageHandler(
      std::make_unique<certificate_manager::CertificatesHandler>());
  web_ui->AddMessageHandler(
      chromeos::cert_provisioning::CertificateProvisioningUiHandler::
          CreateForProfile(profile));
}

CertificateManagerDialogUI::~CertificateManagerDialogUI() {}

}  // namespace ash

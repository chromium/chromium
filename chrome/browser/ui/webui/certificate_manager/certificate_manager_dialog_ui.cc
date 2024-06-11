// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_dialog_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/certificate_manager_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  for (const auto& entry : localized_strings) {
    html_source->AddLocalizedString(entry.name, entry.id);
  }
  certificate_manager::AddLocalizedStrings(html_source);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

CertificateManagerDialogUI::CertificateManagerDialogUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUICertificateManagerHost);
  webui::EnableTrustedTypesCSP(source);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(features::kEnableCertManagementUIV2)) {
    // Serve the old certificate manager
    AddCertificateManagerStrings(source);

    source->UseStringsJs();
    source->SetDefaultResource(IDR_CERT_MANAGER_DIALOG_HTML);

    source->AddBoolean("isGuest",
                       user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
                           user_manager::UserManager::Get()
                               ->IsLoggedInAsManagedGuestSession());
    source->AddBoolean(
        "isKiosk", user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp());

    web_ui->AddMessageHandler(
        std::make_unique<certificate_manager::CertificatesHandler>());
    web_ui->AddMessageHandler(
        chromeos::cert_provisioning::CertificateProvisioningUiHandler::
            CreateForProfile(profile));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kEnableCertManagementUIV2)) {
    // TODO(crbug.com/40928765): Finish serving cert manager v2 here.
    source->UseStringsJs();
    source->AddResourcePath("", IDR_CERT_MANAGER_DIALOG_V2_HTML);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
}

CertificateManagerDialogUI::~CertificateManagerDialogUI() = default;

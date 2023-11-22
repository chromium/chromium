// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager_localized_strings_provider.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace certificate_manager {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"certificateManagerExpandA11yLabel",
     IDS_SETTINGS_CERTIFICATE_MANAGER_EXPAND_ACCESSIBILITY_LABEL},
    {"certificateManagerNoCertificates",
     IDS_SETTINGS_CERTIFICATE_MANAGER_NO_CERTIFICATES},
    {"certificateManagerYourCertificates",
     IDS_SETTINGS_CERTIFICATE_MANAGER_YOUR_CERTIFICATES},
    {"certificateManagerYourCertificatesDescription",
     IDS_SETTINGS_CERTIFICATE_MANAGER_YOUR_CERTIFICATES_DESCRIPTION},
    {"certificateManagerServers", IDS_SETTINGS_CERTIFICATE_MANAGER_SERVERS},
    {"certificateManagerServersDescription",
     IDS_SETTINGS_CERTIFICATE_MANAGER_SERVERS_DESCRIPTION},
    {"certificateManagerAuthorities",
     IDS_SETTINGS_CERTIFICATE_MANAGER_AUTHORITIES},
    {"certificateManagerAuthoritiesDescription",
     IDS_SETTINGS_CERTIFICATE_MANAGER_AUTHORITIES_DESCRIPTION},
    {"certificateManagerOthers", IDS_SETTINGS_CERTIFICATE_MANAGER_OTHERS},
    {"certificateManagerOthersDescription",
     IDS_SETTINGS_CERTIFICATE_MANAGER_OTHERS_DESCRIPTION},
    {"certificateManagerView", IDS_SETTINGS_CERTIFICATE_MANAGER_VIEW},
    {"certificateManagerImport", IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT},
    {"certificateManagerImportAndBind",
     IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_AND_BIND},
    {"certificateManagerExport", IDS_SETTINGS_CERTIFICATE_MANAGER_EXPORT},
    {"certificateManagerDelete", IDS_SETTINGS_DELETE},
    {"certificateManagerUntrusted", IDS_SETTINGS_CERTIFICATE_MANAGER_UNTRUSTED},
    // CA trust edit dialog.
    {"certificateManagerCaTrustEditDialogTitle",
     IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_TITLE},
    {"certificateManagerCaTrustEditDialogDescription",
     IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_DESCRIPTION},
    {"certificateManagerCaTrustEditDialogExplanation",
     IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_EXPLANATION},
    {"certificateManagerCaTrustEditDialogSsl",
     IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_SSL},
    {"certificateManagerCaTrustEditDialogEmail",
     IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_EMAIL},
    {"certificateManagerCaTrustEditDialogObjSign",
     IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_OBJ_SIGN},
    // Certificate delete confirmation dialog.
    {"certificateManagerDeleteUserTitle",
     IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_USER_TITLE},
    {"certificateManagerDeleteUserDescription",
     IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_USER_DESCRIPTION},
    {"certificateManagerDeleteServerTitle",
     IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_SERVER_TITLE},
    {"certificateManagerDeleteServerDescription",
     IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_SERVER_DESCRIPTION},
    {"certificateManagerDeleteCaTitle",
     IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_CA_TITLE},
    {"certificateManagerDeleteCaDescription",
     IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_CA_DESCRIPTION},
    {"certificateManagerDeleteOtherTitle",
     IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_OTHER_TITLE},
    // Encrypt/decrypt password dialogs.
    {"certificateManagerEncryptPasswordTitle",
     IDS_SETTINGS_CERTIFICATE_MANAGER_ENCRYPT_PASSWORD_TITLE},
    {"certificateManagerDecryptPasswordTitle",
     IDS_SETTINGS_CERTIFICATE_MANAGER_DECRYPT_PASSWORD_TITLE},
    {"certificateManagerEncryptPasswordDescription",
     IDS_SETTINGS_CERTIFICATE_MANAGER_ENCRYPT_PASSWORD_DESCRIPTION},
    {"certificateManagerPassword", IDS_SETTINGS_CERTIFICATE_MANAGER_PASSWORD},
    {"certificateManagerConfirmPassword",
     IDS_SETTINGS_CERTIFICATE_MANAGER_CONFIRM_PASSWORD},
    {"certificateImportErrorFormat",
     IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_ERROR_FORMAT},
#if BUILDFLAG(IS_CHROMEOS)
    {"certificateProvisioningListHeader",
     IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_LIST_HEADER},
    {"certificateProvisioningRefresh",
     IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_REFRESH},
    {"certificateProvisioningReset",
     IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_RESET},
    {"certificateProvisioningDetails",
     IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_DETAILS},
    {"certificateProvisioningAdvancedSectionTitle",
     IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_ADVANCED},
    {"certificateProvisioningProfileName",
     IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_CERTIFICATE_PROFILE_NAME},
    {"certificateProvisioningProfileId",
     IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_CERTIFICATE_PROFILE_ID},
    {"certificateProvisioningStatus",
     IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS},
    {"certificateProvisioningStatusId",
     IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_ID},
    {"certificateProvisioningLastUpdate",
     IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_LAST_UPDATE},
    {"certificateProvisioningLastUnsuccessfulStatus",
     IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_LAST_UNSUCCESSFUL_STATUS},
    {"certificateProvisioningPublicKey", IDS_CERT_DETAILS_SUBJECT_KEY},
#endif  // BUILDFLAG(IS_CHROMEOS)
    // For A11y.
    {"menu", IDS_MENU},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace certificate_manager

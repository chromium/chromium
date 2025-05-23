// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"
#include "chrome/browser/ui/webui/certificate_manager/client_cert_sources.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

const char kCRSLearnMoreLink[] =
    "https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/"
    "chrome_root_store/faq.md";

void AddCertificateManagerV2Strings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"ok", IDS_OK},
      {"cancel", IDS_CANCEL},
      {"opensInNewTab", IDS_SETTINGS_OPENS_IN_NEW_TAB},
      {"certificateManagerV2Title", IDS_SETTINGS_CERTIFICATE_MANAGER_V2_TITLE},
      {"certificateManagerV2ClientCerts",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CLIENT_CERTIFICATES},
      {"certificateManagerV2ClientCertsDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CLIENT_CERTIFICATES_DESCRIPTION},
      {"certificateManagerV2ClientCertsFromPlatform",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CLIENT_CERTIFICATES_FROM_PLATFORM},
      {"certificateManagerV2ClientCertsFromExtension",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CLIENT_CERTIFICATES_FROM_EXTENSION},
      {"certificateManagerV2ClientCertsFromAdmin",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CLIENT_CERTIFICATES_FROM_ADMIN},
      {"certificateManagerV2LocalCerts",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_LOCAL_CERTIFICATES},
      {"certificateManagerV2LocalCertsDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_LOCAL_CERTIFICATES_DESCRIPTION},
      {"certificateManagerV2CRSCerts",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CRS_CERTIFICATES},
      {"certificateManagerV2CRSCertsDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CRS_CERTIFICATES_DESCRIPTION},
      {"certificateManagerV2CRSLearnMoreLink",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CRS_LEARN_MORE_LINK},
      {"certificateManagerV2CRSLearnMoreLinkAriaLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CRS_LEARN_MORE_LINK_ARIA_LABEL},
      {"certificateManagerV2HashCopiedToast",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_HASH_COPIED_TOAST},
      {"certificateManagerV2AdminCertsTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_ADMIN_CERTS_TITLE},
      {"certificateManagerV2CustomCertsTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CUSTOM_CERTS_TITLE},
      {"certificateManagerV2TrustedCertsList",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_TRUSTED_CERTS_LIST},
      {"certificateManagerV2IntermediateCertsList",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_INTERMEDIATE_CERTS_LIST},
      {"certificateManagerV2DistrustedCertsList",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DISTRUSTED_CERTS_LIST},
      {"certificateManagerV2NoCertificatesRow",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_NO_CERTIFICATES_ROW},
      {"certificateManagerV2ExportButtonLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_EXPORT_BUTTON_LABEL},
      {"certificateManagerV2ExportButtonAriaLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_EXPORT_BUTTON_ARIA_LABEL},
      {"certificateManagerV2DeleteErrorTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_ERROR_TITLE},
      {"certificateManagerV2ImportErrorTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_ERROR_TITLE},
      {"certificateManagerV2ImportButtonLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_BUTTON_LABEL},
      {"certificateManagerV2ImportButtonAriaLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_BUTTON_ARIA_LABEL},
      {"certificateManagerV2ImportAndBindButtonLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_AND_BIND_BUTTON_LABEL},
      {"certificateManagerV2ImportAndBindButtonAriaLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_AND_BIND_BUTTON_ARIA_LABEL},
      {"certificateManagerV2EnterPasswordTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_ENTER_PASSWORD_TITLE},
      {"certificateManagerV2PlatformCertsTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM_CERTS_TITLE},
      {"certificateManagerV2PlatformCertsToggleLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM_CERTS_TOGGLE_LABEL},
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      {"certificateManagerV2PlatformCertsManageLink",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM_CERTS_MANAGE_LINK},
      {"certificateManagerV2PlatformCertsManageLinkAriaDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM_CERTS_MANAGE_LINK_ARIA_DESCRIPTION},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_CHROMEOS)
      {"certificateProvisioningProcessId",
       IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_PROCESS_ID},
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
      // For ChromeOS provisioning UI
      {"moreActions", IDS_SETTINGS_MORE_ACTIONS},
      {"menu", IDS_MENU},
      {"close", IDS_CLOSE},
#endif  // BUILDFLAG(IS_CHROMEOS)
      {"certificateManagerV2PlatformCertsViewLink",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM_CERTS_VIEW_LINK},
      {"certificateManagerV2Platform",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM},
      {"certificateManagerV2SubpageBackButtonAriaLabel",
       IDS_CERTIFICATE_MANAGER_V2_SUBPAGE_BACK_BUTTON_ARIA_LABEL},
      {"certificateManagerV2SubpageBackButtonAriaRoleDescription",
       IDS_CERTIFICATE_MANAGER_V2_SUBPAGE_BACK_BUTTON_ARIA_ROLE_DESCRIPTION},
      {"certificateManagerV2CertEntryViewAriaLabel",
       IDS_CERTIFICATE_MANAGER_V2_CERT_ENTRY_VIEW_ARIA_LABEL},
      {"certificateManagerV2CertEntryDeleteAriaLabel",
       IDS_CERTIFICATE_MANAGER_V2_CERT_ENTRY_DELETE_ARIA_LABEL},
      {"certificateManagerV2CertHashCopyAriaLabel",
       IDS_CERTIFICATE_MANAGER_V2_CERT_HASH_COPY_ARIA_LABEL},
      {"certificateManagerV2UserCertsTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_USER_CERTS_TITLE},
      {"certificateManagerV2ListExpandAriaLabel",
       IDS_CERTIFICATE_MANAGER_V2_LIST_EXPAND_ARIA_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace

CertificateManagerUI::CertificateManagerUI(content::WebUI* web_ui)
#if BUILDFLAG(IS_CHROMEOS)
    : MojoWebDialogUI(web_ui) {
#else
    : MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
#endif
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUICertificateManagerHost);
  webui::EnableTrustedTypesCSP(source);
  webui::SetJSModuleDefaults(source);

  source->AddResourcePath("", IDR_CERT_MANAGER_DIALOG_V2_HTML);
  AddCertificateManagerV2Strings(source);
  source->AddString("crsLearnMoreUrl", kCRSLearnMoreLink);
#if BUILDFLAG(IS_CHROMEOS)
  ClientCertManagementAccessControls client_cert_policy(profile);
  source->AddBoolean("clientCertImportAllowed",
                     client_cert_policy.IsManagementAllowed(
                         ClientCertManagementAccessControls::kSoftwareBacked));
  source->AddBoolean("clientCertImportAndBindAllowed",
                     client_cert_policy.IsManagementAllowed(
                         ClientCertManagementAccessControls::kHardwareBacked));
  web_ui->AddMessageHandler(
      chromeos::cert_provisioning::CertificateProvisioningUiHandler::
          CreateForProfile(profile));
#endif

  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "certificateManagerV2NumCerts",
      IDS_SETTINGS_CERTIFICATE_MANAGER_V2_NUM_CERTS);
  web_ui->AddMessageHandler(std::move(plural_string_handler));
  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean("userCertsImportAllowed",
                     IsCACertificateManagementAllowed(*prefs));
}

void CertificateManagerUI::BindInterface(
    mojo::PendingReceiver<
        certificate_manager_v2::mojom::CertificateManagerPageHandlerFactory>
        pending_receiver) {
  if (certificate_manager_handler_factory_receiver_.is_bound()) {
    certificate_manager_handler_factory_receiver_.reset();
  }
  certificate_manager_handler_factory_receiver_.Bind(
      std::move(pending_receiver));
}

void CertificateManagerUI::CreateCertificateManagerPageHandler(
    mojo::PendingRemote<certificate_manager_v2::mojom::CertificateManagerPage>
        client,
    mojo::PendingReceiver<
        certificate_manager_v2::mojom::CertificateManagerPageHandler> handler) {
  certificate_manager_page_handler_ =
      std::make_unique<CertificateManagerPageHandler>(
          std::move(client), std::move(handler), Profile::FromWebUI(web_ui()),
          web_ui()->GetWebContents());
}

CertificateManagerUI::~CertificateManagerUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(CertificateManagerUI)
